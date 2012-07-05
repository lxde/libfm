/*
 *      fm-thumbnail.c
 *
 *      Copyright 2010 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* TODO:
 * Thunar can directly load embedded thumbnail in jpeg files, we need that, too.
 * Need to support external thumbnailers.
 * */

#include "fm-thumbnail.h"
#include "fm-config.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef USE_EXIF
#include <libexif/exif-loader.h>
#endif

/* FIXME: this function prototype seems to be missing in header files of GdkPixbuf. Bug report to them. */
gboolean gdk_pixbuf_set_option(GdkPixbuf *pixbuf, const gchar *key, const gchar *value);


/* #define ENABLE_DEBUG */
#ifdef ENABLE_DEBUG
#define DEBUG(...)  g_debug(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

typedef enum
{
    LOAD_NORMAL = 1 << 0, /* need to load normal thumbnail */
    LOAD_LARGE = 1 << 1, /* need to load large thumbnail */
    GENERATE_NORMAL = 1 << 2, /* need to regenerated normal thumbnail */
    GENERATE_LARGE = 1 << 3, /* need to regenerated large thumbnail */
}ThumbnailTaskFlags;

typedef struct _ThumbnailTask ThumbnailTask;
struct _ThumbnailTask
{
    FmFileInfo* fi;         /* never changed between creation and destroying */
    ThumbnailTaskFlags flags; /* used internally */
    sig_atomic_t cancelled; /* no lock required for this type */
    char* uri;              /* used internally */
    char* normal_path;      /* used internally */
    char* large_path;       /* used internally */
    GList* requests;        /* access should be locked */
};

/* members of this structure cannot have concurrent access */
struct _FmThumbnailRequest
{
    FmFileInfo* fi;
    ThumbnailTask* task;
    gboolean size;
    FmThumbnailReadyCallback callback;
    gpointer user_data;
    GdkPixbuf* pix;
};

typedef struct _ThumbnailCacheItem ThumbnailCacheItem;
struct _ThumbnailCacheItem
{
    guint size;
    GdkPixbuf* pix; /* no reference on it */
};

typedef struct _ThumbnailCache ThumbnailCache;
struct _ThumbnailCache
{
    FmPath* path;
    GSList* items;
};

/* FIXME: use thread pool */

/* Lock for loader, generator, and ready queues */
static GStaticRecMutex queue_lock = G_STATIC_REC_MUTEX_INIT;

/* load generated thumbnails */
static GQueue loader_queue = G_QUEUE_INIT; /* consists of ThumbnailTask */
static GThread* loader_thread_id = NULL;
static ThumbnailTask* cur_loading = NULL;

/* generate thumbnails for files */
static GQueue generator_queue = G_QUEUE_INIT; /* consists of ThumbnailTask */
static GThread* generator_thread_id = NULL;
static GCancellable* generator_cancellable = NULL;
static ThumbnailTask* cur_generating = NULL;

/* already loaded thumbnails */
static GQueue ready_queue = G_QUEUE_INIT; /* consists of FmThumbnailRequest */
/* idle handler to call ready callback */
static guint ready_idle_handler = 0;

/* cached thumbnails, elements are ThumbnailCache* */
static GHashTable* hash = NULL;

static char* thumb_dir = NULL;


static gpointer load_thumbnail_thread(gpointer user_data);
static gpointer generate_thumbnail_thread(gpointer user_data);
static void queue_generate(ThumbnailTask* task);
static void load_thumbnails(ThumbnailTask* task);
static void generate_thumbnails_with_gdk_pixbuf(ThumbnailTask* task);
static void generate_thumbnails_with_thumbnailers(ThumbnailTask* task);
static GdkPixbuf* scale_pix(GdkPixbuf* ori_pix, int size);
static void save_thumbnail_to_disk(ThumbnailTask* task, GdkPixbuf* pix, const char* path);

/* may be called in thread */
static void fm_thumbnail_request_free(FmThumbnailRequest* req)
{
    fm_file_info_unref(req->fi);
    if(req->pix)
        g_object_unref(req->pix);
    g_slice_free(FmThumbnailRequest, req);
}

/* should be called with queue lock held */
/* may be called in thread */
inline static void thumbnail_task_free(ThumbnailTask* task)
{
    if(task->requests)
    {
        g_list_foreach(task->requests, (GFunc)fm_thumbnail_request_free, NULL);
        g_list_free(task->requests);
    }
    fm_file_info_unref(task->fi);

    g_free(task->uri);
    g_free(task->normal_path);
    g_free(task->large_path);

    g_slice_free(ThumbnailTask, task);
}

/* in main loop */
static gboolean on_ready_idle(gpointer user_data)
{
    FmThumbnailRequest* req;
    g_static_rec_mutex_lock(&queue_lock);
    while((req = (FmThumbnailRequest*)g_queue_pop_head(&ready_queue)))
    {
        g_static_rec_mutex_unlock(&queue_lock);
        req->callback(req, req->user_data);
        fm_thumbnail_request_free(req);
        g_static_rec_mutex_lock(&queue_lock);
    }
    ready_idle_handler = 0;
    g_static_rec_mutex_unlock(&queue_lock);
    return FALSE;
}

static gint comp_request(gconstpointer a, gconstpointer b)
{
    return ((FmThumbnailRequest*)a)->size - ((FmThumbnailRequest*)b)->size;
}

/* called when cached pixbuf get destroyed */
static void on_pixbuf_destroy(gpointer data, GObject* obj_ptr)
{
    ThumbnailCache* cache = (ThumbnailCache*)data;
    GdkPixbuf* pix = (GdkPixbuf*)obj_ptr;
    GSList* l;
    /* remove it from cache */
    DEBUG("remove from cache!");
    g_static_rec_mutex_lock(&queue_lock);
    for(l=cache->items;l;l=l->next)
    {
        ThumbnailCacheItem* item = (ThumbnailCacheItem*)l->data;
        if(item->pix == pix)
        {
            cache->items = g_slist_delete_link(cache->items, l);
            g_slice_free(ThumbnailCacheItem, item);
            if(!cache->items)
            {
                if(hash) /* it could be already destroyed */
                    g_hash_table_remove(hash, cache->path);
                fm_path_unref(cache->path);
                g_slice_free(ThumbnailCache, cache);
            }
            break;
        }
    }
    g_static_rec_mutex_unlock(&queue_lock);
}

/* called with queue lock held */
/* in thread */
inline static void cache_thumbnail_in_hash(FmPath* path, GdkPixbuf* pix, guint size)
{
    ThumbnailCache* cache;
    ThumbnailCacheItem* item;
    GSList* l = NULL;
    cache = (ThumbnailCache*)g_hash_table_lookup(hash, path);
    if(cache)
    {
        for(l=cache->items;l;l=l->next)
        {
            item = (ThumbnailCacheItem*)l->data;
            if(item->size == size)
                break;
        }
    }
    else
    {
        cache = g_slice_new0(ThumbnailCache);
        cache->path = fm_path_ref(path);
        g_hash_table_insert(hash, cache->path, cache);
    }
    if(!l) /* the item is not in cache->items */
    {
        item = g_slice_new(ThumbnailCacheItem);
        item->size = size;
        item->pix = pix;
        cache->items = g_slist_prepend(cache->items, item);
        g_object_weak_ref(G_OBJECT(pix), on_pixbuf_destroy, cache);
    }
}

/* called with queue lock held */
/* in thread */
static void thumbnail_task_finish(ThumbnailTask* task, GdkPixbuf* normal_pix, GdkPixbuf* large_pix)
{
    GdkPixbuf* cached_pix = NULL;
    gint cached_size = 0;
    GList* l;

    /* sort the requests by requested size to utilize cached scaled pixbuf */
    task->requests = g_list_sort(task->requests, comp_request);
    for(l=task->requests; l; l=l->next)
    {
        FmThumbnailRequest* req = (FmThumbnailRequest*)l->data;
        /* the thumbnail is ready, queue the request in ready queue. */
        /* later, the ready callbacks will be called in idle handler of main thread. */
        if(req->size == cached_size)
        {
            req->pix = cached_pix ? (GdkPixbuf*)g_object_ref(cached_pix) : NULL;
            DEBUG("cache hit!");
            g_queue_push_tail(&ready_queue, req);
            if( 0 == ready_idle_handler ) /* schedule an idle handler if there isn't one. */
                ready_idle_handler = g_idle_add_full(G_PRIORITY_LOW, on_ready_idle, NULL, NULL);
            continue;
        }

        if(G_LIKELY(req->size <= 128)) /* normal */
        {
            if(normal_pix)
                req->pix = scale_pix(normal_pix, req->size);
            else
                req->pix = NULL;
        }
        else /* large */
        {
            if(large_pix)
                req->pix = scale_pix(large_pix, req->size);
            else
                req->pix = NULL;
        }

        if(cached_pix)
            g_object_unref(cached_pix);
        cached_pix = req->pix ? g_object_ref(req->pix) : NULL;
        cached_size = req->size;

        /* cache this in hash table */
        if(cached_pix)
            cache_thumbnail_in_hash(fm_file_info_get_path(req->fi), cached_pix, cached_size);

        g_queue_push_tail(&ready_queue, req);
        if( 0 == ready_idle_handler ) /* schedule an idle handler if there isn't one. */
            ready_idle_handler = g_idle_add_full(G_PRIORITY_LOW, on_ready_idle, NULL, NULL);
    }
    if(cached_pix)
        g_object_unref(cached_pix);

    g_list_free(task->requests);
    task->requests = NULL;
}

/* in thread */
static gboolean is_thumbnail_outdated(GdkPixbuf* thumb_pix, const char* thumbnail_path, time_t mtime)
{
    const char* thumb_mtime = gdk_pixbuf_get_option(thumb_pix, "tEXt::Thumb::MTime");
    gboolean outdated = FALSE;
    if(thumb_mtime)
    {
        if(atol(thumb_mtime) != mtime)
            outdated = TRUE;
    }
    else
    {
        /* if the thumbnail png file does not contain "tEXt::Thumb::MTime" value,
         * we compare the mtime of the thumbnail with its original directly. */
        struct stat statbuf;
        if(stat(thumbnail_path, &statbuf) == 0) /* get mtime of the thumbnail file */
        {
            if(mtime > statbuf.st_mtime)
                outdated = TRUE;
        }
    }

    /* out of date, delete it */
    if(outdated)
    {
        unlink(thumbnail_path); /* delete the out-dated thumbnail. */
        g_object_unref(thumb_pix);
    }
    return outdated;
}

/* in thread */
static void load_thumbnails(ThumbnailTask* task)
{
    GdkPixbuf* normal_pix = NULL;
    GdkPixbuf* large_pix = NULL;
    const char* normal_path = task->normal_path;
    const char* large_path = task->large_path;

    if( task->cancelled )
        goto _out;

    DEBUG("loading: %s, %s", fm_file_info_get_name(task->fi), normal_path);

    if(task->flags & LOAD_NORMAL)
    {
        normal_pix = gdk_pixbuf_new_from_file(normal_path, NULL);
        if(!normal_pix || is_thumbnail_outdated(normal_pix, normal_path, fm_file_info_get_mtime(task->fi)))
        {
            /* normal_pix is freed in is_thumbnail_outdated() if it's out of date. */
            /* generate normal size thumbnail */
            task->flags |= GENERATE_NORMAL;
            normal_pix = NULL;
            /* DEBUG("need to generate normal thumbnail"); */
        }
        else
        {
            DEBUG("normal thumbnail loaded: %p", normal_pix);
        }
    }

    if( task->cancelled )
        goto _out;

    if(task->flags & LOAD_LARGE)
    {
        large_pix = gdk_pixbuf_new_from_file(large_path, NULL);
        if(!large_pix || is_thumbnail_outdated(large_pix, large_path, fm_file_info_get_mtime(task->fi)))
        {
            /* large_pix is freed in is_thumbnail_outdated() if it's out of date. */
            /* generate large size thumbnail */
            task->flags |= GENERATE_LARGE;
            large_pix = NULL;
        }
    }

    if( task->cancelled )
        goto _out;

    if(task->flags & (GENERATE_NORMAL|GENERATE_LARGE)) /* need to re-generate some thumbnails */
    {
        GList* generate_reqs = NULL, *l;
        ThumbnailTask* generate_task;

        /* remove all requests which requires re-generating thumbnails from task and gather them in a list */
        g_static_rec_mutex_lock(&queue_lock); /* no request should be changed or left out without lock */
        for(l=task->requests; l; )
        {
            FmThumbnailRequest* req = (FmThumbnailRequest*)l->data;
            GList* next = l->next;
            if(req->size <= 128) /* need normal thumbnail */
            {
                if(task->flags & GENERATE_NORMAL)
                {
                    task->requests = g_list_remove_link(task->requests, l);
                    generate_reqs = g_list_concat(generate_reqs, l);
                }
            }
            else /* need large thumbnail */
            {
                if(task->flags & GENERATE_LARGE)
                {
                    task->requests = g_list_remove_link(task->requests, l);
                    generate_reqs = g_list_concat(generate_reqs, l);
                }
            }
            l = next;
        }

        /* this list contains requests requiring regeration of thumbnails */
        if(generate_reqs)
        {
            generate_task = g_slice_new0(ThumbnailTask);
            generate_task->flags = task->flags;
            generate_task->fi = fm_file_info_ref(task->fi);
            generate_task->requests = generate_reqs;
            generate_task->uri = g_strdup(task->uri);
            generate_task->normal_path = g_strdup(task->normal_path);
            generate_task->large_path = g_strdup(task->large_path);
            DEBUG("queue regenerate for :%s", fm_file_info_get_name(task->fi));
            /* queue the re-generation task */
            queue_generate(generate_task); /* consumes the task */
        }
        g_static_rec_mutex_unlock(&queue_lock);
    }

_out:
    g_static_rec_mutex_lock(&queue_lock);
    /* thumbnails which don't require re-generation should all be loaded at this point. */
    if(!task->cancelled && task->requests)
        thumbnail_task_finish(task, normal_pix, large_pix);
    cur_loading = NULL;
    /* task is freed in thumbnail_task_finish() */
    g_static_rec_mutex_unlock(&queue_lock);

    if(normal_pix)
        g_object_unref(normal_pix);
    if(large_pix)
        g_object_unref(large_pix);

    return;
}

/* in thread */
static gpointer load_thumbnail_thread(gpointer user_data)
{
    ThumbnailTask* task;
    GChecksum* sum = g_checksum_new(G_CHECKSUM_MD5);
    gchar* normal_path  = g_build_filename(thumb_dir, "normal/00000000000000000000000000000000.png", NULL);
    gchar* normal_basename = strrchr(normal_path, '/') + 1;
    gchar* large_path = g_build_filename(thumb_dir, "large/00000000000000000000000000000000.png", NULL);
    gchar* large_basename = strrchr(large_path, '/') + 1;

    /* ensure thumbnail directories exists */
    g_mkdir_with_parents(normal_path, 0700);
    g_mkdir_with_parents(large_path, 0700);

    for(;;)
    {
        g_static_rec_mutex_lock(&queue_lock);
        task = g_queue_pop_head(&loader_queue);
        cur_loading = task;
        if(G_LIKELY(task))
        {
            char* uri;
            const char* md5;

            g_static_rec_mutex_unlock(&queue_lock);
            uri = fm_path_to_uri(fm_file_info_get_path(task->fi));

            /* generate filename for the thumbnail */
            g_checksum_update(sum, (guchar*)uri, -1);
            md5 = g_checksum_get_string(sum); /* md5 sum of the URI */

            task->uri = uri;

            if (task->flags & LOAD_NORMAL)
            {
                memcpy( normal_basename, md5, 32 );
                task->normal_path = normal_path;
            }
            if (task->flags & LOAD_LARGE)
            {
                memcpy( large_basename, md5, 32 );
                task->large_path = large_path;
            }

            load_thumbnails(task); /* resets cur_loading */

            g_checksum_reset(sum);
            task->uri = NULL;
            task->normal_path = NULL;
            task->large_path = NULL;
            g_free(uri);
            /* no lock here as task is already unaccesible */
            thumbnail_task_free(task);
        }
        else /* no task is left in the loader_queue */
        {
            loader_thread_id = NULL;
            g_static_rec_mutex_unlock(&queue_lock);
            break;
        }
    }
    g_free(normal_path);
    g_free(large_path);
    g_checksum_free(sum);
    return NULL;
}

/* should be called with queue locked */
/* in main loop */
inline static GdkPixbuf* find_thumbnail_in_hash(FmPath* path, guint size)
{
    ThumbnailCache* cache = (ThumbnailCache*)g_hash_table_lookup(hash, path);
    if(cache)
    {
        GSList* l;
        for(l=cache->items;l;l=l->next)
        {
            ThumbnailCacheItem* item = (ThumbnailCacheItem*)l->data;
            if(item->size == size)
                return item->pix;
        }
    }
    return NULL;
}

/* should be called with queue locked */
/* may be called in thread */
static ThumbnailTask* find_queued_task(GQueue* queue, FmFileInfo* fi)
{
    GList* l;
    for( l = queue->head; l; l=l->next )
    {
        ThumbnailTask* task = (ThumbnailTask*)l->data;
        if(G_UNLIKELY(task->fi == fi || fm_path_equal(fm_file_info_get_path(task->fi), fm_file_info_get_path(fi))))
            return task;
    }
    return NULL;
}

/* in main loop */
FmThumbnailRequest* fm_thumbnail_request(FmFileInfo* src_file,
                                         guint size,
                                         FmThumbnailReadyCallback callback,
                                         gpointer user_data)
{
    FmThumbnailRequest* req;
    ThumbnailTask* task;
    GdkPixbuf* pix;
    FmPath* src_path = fm_file_info_get_path(src_file);

    g_return_val_if_fail(hash != NULL, NULL);
    req = g_slice_new(FmThumbnailRequest);
    req->fi = fm_file_info_ref(src_file);
    req->size = size;
    req->callback = callback;
    req->user_data = user_data;
    req->pix = NULL;

    DEBUG("request thumbnail: %s", fm_path_get_basename(src_path));

    g_static_rec_mutex_lock(&queue_lock);

    /* FIXME: find in the cache first to see if thumbnail is already cached */
    pix = find_thumbnail_in_hash(src_path, size);
    if(pix)
    {
        DEBUG("cache found!");
        req->pix = (GdkPixbuf*)g_object_ref(pix);
        /* call the ready callback in main loader_thread_id from idle handler. */
        g_queue_push_tail(&ready_queue, req);
        if( 0 == ready_idle_handler ) /* schedule an idle handler if there isn't one. */
            ready_idle_handler = g_idle_add_full(G_PRIORITY_LOW, on_ready_idle, NULL, NULL);
        g_static_rec_mutex_unlock(&queue_lock);
        return req;
    }

    /* if it's not cached, add it to the loader_queue for loading. */
    task = find_queued_task(&loader_queue, src_file);
    if(!task)
        task = find_queued_task(&generator_queue, src_file);

    if(!task)
    {
        task = g_slice_new0(ThumbnailTask);
        task->fi = fm_file_info_ref(src_file);
        g_queue_push_tail(&loader_queue, task);
    }
    else
    {
        DEBUG("task already in the queue: %p", task);
    }

    if(size > 128)
        task->flags |= LOAD_LARGE;
    else
        task->flags |= LOAD_NORMAL;

    task->requests = g_list_append(task->requests, req);

    if(!loader_thread_id)
        loader_thread_id = g_thread_create( load_thumbnail_thread, NULL, FALSE, NULL);

    g_static_rec_mutex_unlock(&queue_lock);
    return req;
}

/* in main loop */
void fm_thumbnail_request_cancel(FmThumbnailRequest* req)
{
    ThumbnailTask* task;
    GList* l, *l2;

    g_return_if_fail(req != NULL);
    g_static_rec_mutex_lock(&queue_lock);
    /* if it's in generator queue (most likely) */

    if(cur_generating && cur_generating->requests)
    {
        /* this is the currently processed item */
        l2 = g_list_find(cur_generating->requests, req);
        if(l2)
        {
            cur_generating->requests = g_list_delete_link(cur_generating->requests, l2);
            if(!cur_generating->requests)
            {
                /* cur_generating->cancelled = TRUE; */
                if(generator_cancellable)
                    g_cancellable_cancel(generator_cancellable);
            }
            goto done;
        }
    }

    for(l=generator_queue.head; l; l=l->next)
    {
        task = (ThumbnailTask*)l->data;
        l2 = g_list_find(task->requests, req);
        if(l2) /* found the request */
        {
            task->requests = g_list_delete_link(task->requests, l2);
            if(!task->requests) /* no one is requesting this thumbnail */
            {
                {
                    g_queue_delete_link(&generator_queue, l);
                    thumbnail_task_free(task);
                }
            }
            goto done;
        }
    }

    /* not found, try loader queue */

    if(cur_loading && cur_loading->requests)
    {
        /* this is the currently processed item */
        l2=g_list_find(cur_loading->requests, req);
        if(l2)
        {
            cur_loading->requests = g_list_delete_link(cur_loading->requests, l2);
            if(!cur_loading->requests)
                cur_loading->cancelled = TRUE;
            goto done;
        }
    }

    for(l=loader_queue.head; l; l=l->next)
    {
        task = (ThumbnailTask*)l->data;
        l2 = g_list_find(task->requests, req);
        if(l2) /* found the request */
        {
            task->requests = g_list_delete_link(task->requests, l2);
            if(!task->requests) /* no one is requesting this thumbnail */
            {
                g_queue_delete_link(&loader_queue, l);
                thumbnail_task_free(task);
            }
            goto done;
        }
    }

    /* not found in both loader or generator queue */
    /* is it in ready queue? */
    l = g_queue_find(&ready_queue, req);
    if(l)
    {
        g_queue_delete_link(&ready_queue, l);
        fm_thumbnail_request_free(req);
        /* if there is no item left in ready queue, cancel idle handler */
        if(g_queue_is_empty(&ready_queue) && ready_idle_handler)
        {
            g_source_remove(ready_idle_handler);
            ready_idle_handler = 0;
        }
    }
done:
    g_static_rec_mutex_unlock(&queue_lock);
}

/* in main loop */
GdkPixbuf* fm_thumbnail_request_get_pixbuf(FmThumbnailRequest* req)
{
    return req->pix;
}

/* in main loop */
FmFileInfo* fm_thumbnail_request_get_file_info(FmThumbnailRequest* req)
{
    return req->fi;
}

/* in main loop */
guint fm_thumbnail_request_get_size(FmThumbnailRequest* req)
{
    return req->size;
}

/* in main loop */
void _fm_thumbnail_init()
{
    thumb_dir = g_build_filename(g_get_home_dir(), ".thumbnails", NULL);
    hash = g_hash_table_new((GHashFunc)fm_path_hash, (GEqualFunc)fm_path_equal);
}

/* in main loop */
void _fm_thumbnail_finalize()
{
    ThumbnailTask* task;
    FmThumbnailRequest* req;

    g_static_rec_mutex_lock(&queue_lock);
    /* cancel all pending requests before destroying hash */
    while((task = g_queue_pop_head(&loader_queue)))
        thumbnail_task_free(task);
    while((task = g_queue_pop_head(&generator_queue)))
        thumbnail_task_free(task);
    /* if threads were alive its will die after that */
    while((req = g_queue_pop_head(&ready_queue)))
        fm_thumbnail_request_free(req);
    g_hash_table_destroy(hash); /* caches will be destroyed by pixbufs */
    hash = NULL;
    g_static_rec_mutex_unlock(&queue_lock);
    g_free(thumb_dir);
}

/* in thread */
static gpointer generate_thumbnail_thread(gpointer user_data)
{
    ThumbnailTask* task;
    g_static_rec_mutex_lock(&queue_lock);
    generator_cancellable = g_cancellable_new();
    for(;;)
    {
        task = g_queue_pop_head(&generator_queue);
        DEBUG("pop task from generator queue: %p", task);

        if( G_LIKELY(task) )
        {
            cur_generating = task;
            g_static_rec_mutex_unlock(&queue_lock);

            if(fm_file_info_is_image(task->fi))
            {
                /* FIXME: if the built-in thumbnail generation fails
                 * still call external thumbnailer to handle it.
                 *
                 * We should only handle those mime-types supported
                 * by GdkPixbuf listed by gdk_pixbuf_get_formats(). */
                /* if the image file is too large, don't generate thumbnail for it. */
                if(fm_file_info_get_size(task->fi) <= (fm_config->thumbnail_max << 10))
                    generate_thumbnails_with_gdk_pixbuf(task);
                /* FIXME: should requestor be informed we not loaded thumbnail? */
            }
            else
                generate_thumbnails_with_thumbnailers(task);

            g_static_rec_mutex_lock(&queue_lock);
            cur_generating = NULL;
            thumbnail_task_free(task);
            if(g_cancellable_is_cancelled(generator_cancellable))
            {
                DEBUG("generation of thumbnail is cancelled!");
                g_cancellable_reset(generator_cancellable);
                break;
            }
        }
        else
        {
            DEBUG("no task is in generator queue, exit generator thread");
            break;
        }
    }
    generator_thread_id = NULL;
    g_object_unref(generator_cancellable);
    generator_cancellable = NULL;
    g_static_rec_mutex_unlock(&queue_lock);
    return NULL;
}

/* should be called with queue locked */
/* in thread */
/* will destroy the task or move it into generator queue */
static void queue_generate(ThumbnailTask* regenerate_task)
{
    ThumbnailTask* task;
    task = find_queued_task(&generator_queue, regenerate_task->fi);
    if(task)
    {
        task->flags |= regenerate_task->flags;
        task->requests = g_list_concat(task->requests, regenerate_task->requests);
        regenerate_task->requests = NULL;
        thumbnail_task_free(regenerate_task);

        return;
    }
    DEBUG("push into generator queue");
    g_queue_push_tail(&generator_queue, regenerate_task);

    if(!generator_thread_id)
        generator_thread_id = g_thread_create(generate_thumbnail_thread, NULL, FALSE, NULL);
}

/* in thread */
static GdkPixbuf* scale_pix(GdkPixbuf* ori_pix, int size)
{
    GdkPixbuf* scaled_pix;
    /* keep aspect ratio and scale to thumbnail size: 128 or 256 */
    int width = gdk_pixbuf_get_width(ori_pix);
    int height = gdk_pixbuf_get_height(ori_pix);
    int new_width;
    int new_height;

    if(width > height)
    {
        gdouble aspect = (gdouble)height / width;
        new_width = size;
        new_height = size * aspect;
    }
    else if(width < height)
    {
        gdouble aspect = (gdouble)width / height;
        new_height = size;
        new_width = size * aspect;
    }
    else
    {
        new_width = new_height = size;
    }

    if((new_width == width && new_height == height) ||
       (size > width && size > height )) /* don't scale up */
    {
        /* if size is not changed or original size is smaller, use original size. */
        scaled_pix = (GdkPixbuf*)g_object_ref(ori_pix);
    }
    else
        scaled_pix = gdk_pixbuf_scale_simple(ori_pix, new_width, new_height, GDK_INTERP_BILINEAR);

    return scaled_pix;
}

/* in thread */
static void save_thumbnail_to_disk(ThumbnailTask* task, GdkPixbuf* pix, const char* path)
{
    /* save the generated thumbnail to disk */
    char* tmpfile = g_strconcat(path, ".XXXXXX", NULL);
    gint fd;
    fd = g_mkstemp(tmpfile); /* save to a temp file first */
    if(fd != -1)
    {
        char mtime_str[100];
        g_snprintf( mtime_str, 100, "%lu", fm_file_info_get_mtime(task->fi));
        chmod( tmpfile, 0600 );  /* only the owner can read it. */
        gdk_pixbuf_save( pix, tmpfile, "png", NULL,
                         "tEXt::Thumb::URI", task->uri,
                         "tEXt::Thumb::MTime", mtime_str, NULL );
        close(fd);
        g_rename(tmpfile, path);
        g_free(tmpfile);
    }
    DEBUG("generator: save to %s", path);
}

/* in thread */
static void generate_thumbnails_with_gdk_pixbuf(ThumbnailTask* task)
{
    /* FIXME: only formats supported by GdkPixbuf should be handled this way. */
    GFile* gf = fm_path_to_gfile(fm_file_info_get_path(task->fi));
    GFileInputStream* ins;
    GdkPixbuf* normal_pix = NULL;
    GdkPixbuf* large_pix = NULL;

    DEBUG("generate thumbnail for %s", fm_file_info_get_name(task->fi));

    ins = g_file_read(gf, generator_cancellable, NULL);
    if(ins)
    {
        GdkPixbuf* ori_pix = NULL;
#ifdef USE_EXIF
        /* use libexif to extract thumbnails embedded in jpeg files */
        FmMimeType* mime_type = fm_file_info_get_mime_type(task->fi);
        if(strcmp(mime_type->type, "image/jpeg") == 0) /* if this is a jpeg file */
        {
            /* try to extract thumbnails embedded in jpeg files */
            ExifLoader *exif_loader = exif_loader_new();
            ExifData *exif_data;
            while(!g_cancellable_is_cancelled(generator_cancellable)) {
                char buf[4096];
                gssize read_size = g_input_stream_read(ins, buf, 4096, generator_cancellable, NULL);
                if(read_size == 0) /* EOF */
                    break;
                if(exif_loader_write(exif_loader, buf, read_size) == 0)
                    break; /* no more EXIF data */
            }
            exif_data = exif_loader_get_data(exif_loader);
            exif_loader_unref(exif_loader);
            if(exif_data)
            {
                if(exif_data->data) /* if an embedded thumbnail is available */
                {
                    /* load the embedded jpeg thumbnail */
                    GInputStream* mem_stream = g_memory_input_stream_new_from_data(exif_data->data, exif_data->size, NULL);
                    ori_pix = gdk_pixbuf_new_from_stream(mem_stream, generator_cancellable, NULL);
                    /* FIXME: how to apply orientation tag for this? maybe use libexif? */
                    g_object_unref(mem_stream);
                }
                exif_data_unref(exif_data);
            }
        }

        if(!ori_pix)
        {
            /* FIXME: instead of reload the image file again, it's posisble to get the bytes
             * read already by libexif with exif_loader_get_buf() and feed the data to
             * GdkPixbufLoader ourselves. However the performance improvement by doing this
             * might be negliable, I think. */
            GSeekable* seekable = G_SEEKABLE(ins);
            if(g_seekable_can_seek(seekable))
            {
                /* an EXIF thumbnail is not found, lets rewind the file pointer to beginning of
                 * the file and load the image with gdkpixbuf instead. */
                g_seekable_seek(seekable, 0, G_SEEK_SET, generator_cancellable, NULL);
            }
            else
            {
                /* if the stream is not seekable, close it and open it again. */
                g_input_stream_close(G_INPUT_STREAM(ins), NULL, NULL);
                g_object_unref(ins);
                ins = g_file_read(gf, generator_cancellable, NULL);
            }
            ori_pix = gdk_pixbuf_new_from_stream(G_INPUT_STREAM(ins), generator_cancellable, NULL);
        }
#else
        ori_pix = gdk_pixbuf_new_from_stream(G_INPUT_STREAM(ins), generator_cancellable, NULL);
#endif
        g_input_stream_close(G_INPUT_STREAM(ins), NULL, NULL);
        g_object_unref(ins);

        if(ori_pix) /* if the original image is successfully loaded */
        {
            const char* orientation_str = gdk_pixbuf_get_option(ori_pix, "orientation");
            int width = gdk_pixbuf_get_width(ori_pix);
            int height = gdk_pixbuf_get_height(ori_pix);
            gboolean need_save;

            if(task->flags & GENERATE_NORMAL)
            {
                /* don't create thumbnails for images which are too small */
                if(width <=128 && height <= 128)
                {
                    normal_pix = (GdkPixbuf*)g_object_ref(ori_pix);
                    need_save = FALSE;
                }
                else
                {
                    normal_pix = scale_pix(ori_pix, 128);
                    need_save = TRUE;
                }
                if(orientation_str)
                {
                    GdkPixbuf* rotated;
                    gdk_pixbuf_set_option(normal_pix, "orientation", orientation_str);
                    rotated = gdk_pixbuf_apply_embedded_orientation(normal_pix);
                    g_object_unref(normal_pix);
                    normal_pix = rotated;
                }
                if(need_save)
                    save_thumbnail_to_disk(task, normal_pix, task->normal_path);
            }

            if(task->flags & GENERATE_LARGE)
            {
                /* don't create thumbnails for images which are too small */
                if(width <=256 && height <= 256)
                {
                    large_pix = (GdkPixbuf*)g_object_ref(ori_pix);
                    need_save = FALSE;
                }
                else
                {
                    large_pix = scale_pix(ori_pix, 256);
                    need_save = TRUE;
                }
                if(orientation_str)
                {
                    GdkPixbuf* rotated;
                    gdk_pixbuf_set_option(large_pix, "orientation", orientation_str);
                    rotated = gdk_pixbuf_apply_embedded_orientation(large_pix);
                    g_object_unref(large_pix);
                    large_pix = rotated;
                }
                if(need_save)
                    save_thumbnail_to_disk(task, large_pix, task->large_path);
            }
            g_object_unref(ori_pix);
        }
    }

    g_static_rec_mutex_lock(&queue_lock);
    thumbnail_task_finish(task, normal_pix, large_pix);
    g_static_rec_mutex_unlock(&queue_lock);

    if(normal_pix)
        g_object_unref(normal_pix);
    if(large_pix)
        g_object_unref(large_pix);

    g_object_unref(gf);
}

/* in thread */
static void generate_thumbnails_with_thumbnailers(ThumbnailTask* task)
{
    /* external thumbnailer support */
    GdkPixbuf* normal_pix = NULL;
    GdkPixbuf* large_pix = NULL;
    FmMimeType* mime_type = fm_file_info_get_mime_type(task->fi);
    /* TODO: we need to add timeout for external thumbnailers.
     * If a thumbnailer program is broken or locked for unknown reason,
     * the thumbnailer process should be killed once a timeout is reached. */
    if(mime_type)
    {
        const GList* thumbnailers = fm_mime_type_get_thumbnailers(mime_type);
        const GList* l;
        guint generated = 0;
        for(l = thumbnailers; l; l = l->next)
        {
            FmThumbnailer* thumbnailer = FM_THUMBNAILER(l->data);
            DEBUG("generate thumbnail with: %s", thumbnailer->id);
            if((task->flags & GENERATE_NORMAL) && !(generated & GENERATE_NORMAL))
            {
                if(fm_thumbnailer_launch_for_uri(thumbnailer, task->uri, task->normal_path, 128))
                {
                    generated |= GENERATE_NORMAL;
                    normal_pix = gdk_pixbuf_new_from_file(task->normal_path, NULL);
                }
            }
            if((task->flags & GENERATE_LARGE) && !(generated & GENERATE_LARGE))
            {
                if(fm_thumbnailer_launch_for_uri(thumbnailer, task->uri, task->large_path, 256))
                {
                    generated |= GENERATE_LARGE;
                    large_pix = gdk_pixbuf_new_from_file(task->normal_path, NULL);
                }
            }

            /* if both large and normal thumbnails are generated, quit */
            if(generated == task->flags)
                break;
        }
    }
    g_static_rec_mutex_lock(&queue_lock);
    thumbnail_task_finish(task, normal_pix, large_pix);
    g_static_rec_mutex_unlock(&queue_lock);

    if(normal_pix)
        g_object_unref(normal_pix);
    if(large_pix)
        g_object_unref(large_pix);
}
