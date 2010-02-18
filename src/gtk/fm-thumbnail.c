/*
 *      fm-thumbnail.c
 *      
 *      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include "fm-thumbnail.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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
    CANCEL = 1 << 4, /* the task is cancelled */
    ALLOC_STRINGS = 1 << 5, /* uri, normal_path, and large_path are dynamically allocated and needs to be freed later */
}ThumbnailTaskFlags;

typedef struct _ThumbnailTask ThumbnailTask;
struct _ThumbnailTask
{
    FmFileInfo* fi;
    ThumbnailTaskFlags flags;
    char* uri;
    char* normal_path;
    char* large_path;
    GList* requests;
};

#define IS_CANCELLED(task)  (task->flags & CANCEL)

struct _FmThumbnailRequest
{
    FmFileInfo* fi;
    ThumbnailTask* task;
    gboolean size;
    FmThumbnailReadyCallback callback;
    gpointer user_data;
    GdkPixbuf* pix;
};

/* FIXME: use thread pool */

G_LOCK_DEFINE_STATIC(queue);

/* load generated thumbnails */
static GQueue loader_queue = G_QUEUE_INIT;
static GThread* loader_thread_id = NULL;

/* generate thumbnails for files */
static GQueue generator_queue = G_QUEUE_INIT;
static GThread* generator_thread_id = NULL;

/* already loaded thumbnails */
static GQueue ready_queue = G_QUEUE_INIT;
/* idle handler to call ready callback */
static guint ready_idle_handler = 0;


/* cached thumbnails */
static guint* sizes = NULL;
static GHashTable* hashes = NULL;

static GCancellable* generator_cancellable = NULL;

static char* thumb_dir = NULL;

static ThumbnailTask* find_queued_task(GQueue* queue, FmFileInfo* fi);
static gpointer load_thumbnail_thread(gpointer user_data);
static gpointer generate_thumbnail_thread(gpointer user_data);
static void thumbnail_task_finish(ThumbnailTask* task, GdkPixbuf* normal_pix, GdkPixbuf* large_pix);
static void queue_generate(ThumbnailTask* task);
static void load_thumbnails(ThumbnailTask* task);
static void generate_thumbnails_with_gdk_pixbuf(ThumbnailTask* task);
static void generate_thumbnails_with_thumbnailers(ThumbnailTask* task);
inline static GdkPixbuf* scale_pix(GdkPixbuf* ori_pix, int size);
static void save_thumbnail_to_disk(ThumbnailTask* task, GdkPixbuf* pix, const char* path);

static void fm_thumbnail_request_free(FmThumbnailRequest* req)
{
    if(req->pix)
        g_object_unref(req->pix);
    g_slice_free(FmThumbnailRequest, req);
}

inline static void thumbnail_task_free(ThumbnailTask* task)
{
    if(task->requests)
    {
        g_list_foreach(task->requests, (GFunc)fm_thumbnail_request_free, NULL);
        g_list_free(task->requests);
    }
    fm_file_info_unref(task->fi);

    /* if those strings are dynamically allocated, free them. */    
    if(task->flags & ALLOC_STRINGS)
    {
        g_free(task->uri);
        g_free(task->normal_path);
        g_free(task->large_path);
    }

    g_slice_free(ThumbnailTask, task);
}

static gboolean on_ready_idle(gpointer user_data)
{
    FmThumbnailRequest* req;
    G_LOCK(queue);
    while( req = (FmThumbnailRequest*)g_queue_pop_head(&ready_queue) )
    {
        // GDK_THREADS_ENTER();
        req->callback(req, req->user_data);
        // GDK_THREADS_LEAVE();
        fm_thumbnail_request_free(req);
    }
    ready_idle_handler = 0;
    G_UNLOCK(queue);
    return FALSE;
}

static gint comp_request(FmThumbnailRequest* a, FmThumbnailRequest* b)
{
    return a->size - b->size;
}

/* called with queue lock held */
void thumbnail_task_finish(ThumbnailTask* task, GdkPixbuf* normal_pix, GdkPixbuf* large_pix)
{
    GdkPixbuf* cached_pix = NULL;
    gint cached_size = 0;
    GList* l;

    /* sort the requests by requested size to utilize cached scaled pixbuf */
    task->requests = g_list_sort(task->requests, (GCompareFunc)comp_request);
    for(l=task->requests; l; l=l->next)
    {
        FmThumbnailRequest* req = (FmThumbnailRequest*)l->data;
        /* the thumbnail is ready, queue the request in ready queue. */
        /* later, the ready callbacks will be called in idle handler of main thread. */
        if(req->size == cached_size)
        {
            req->pix = cached_pix ? (GdkPixbuf*)g_object_ref(cached_pix) : NULL;
            DEBUG("cache hit!");
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

        g_queue_push_tail(&ready_queue, req);
        if( 0 == ready_idle_handler ) /* schedule an idle handler if there isn't one. */
            ready_idle_handler = g_idle_add_full(G_PRIORITY_LOW, on_ready_idle, NULL, NULL);
    }
    if(cached_pix)
        g_object_unref(cached_pix);

    g_list_free(task->requests);
    task->requests = NULL;
    thumbnail_task_free(task);
}

inline static gboolean is_thumbnail_outdated(GdkPixbuf* thumb_pix, const char* path, time_t mtime)
{
    const char* thumb_mtime = gdk_pixbuf_get_option(thumb_pix, "tEXt::Thumb::MTime");
    /* out of date, delete it */
    if( !thumb_mtime || atol(thumb_mtime) != mtime )
    {
        unlink(path); /* delete the out-dated thumbnail. */
        g_object_unref(thumb_pix);
    }
}

void load_thumbnails(ThumbnailTask* task)
{
    GList* l;
    GdkPixbuf* normal_pix = NULL;
    GdkPixbuf* large_pix = NULL;
    const char* normal_path = task->normal_path;
    const char* large_path = task->large_path;

    if(task->flags & LOAD_NORMAL)
    {
        normal_pix = gdk_pixbuf_new_from_file(normal_path, NULL);
        if(!normal_pix || is_thumbnail_outdated(normal_pix, normal_path, task->fi->mtime))
        {
            /* normal_pix is freed in is_thumbnail_outdated() if it's out of date. */
            /* generate normal size thumbnail */
            task->flags |= GENERATE_NORMAL;
            normal_pix = NULL;
            /* DEBUG("need to generate normal thumbnail"); */
        }
        else
            DEBUG("normal thumbnail loaded: %p", normal_pix);
    }
    
    if( IS_CANCELLED(task) )
        goto _out;

    if(task->flags & LOAD_LARGE)
    {
        large_pix = gdk_pixbuf_new_from_file(large_path, NULL);
        if(!large_pix || is_thumbnail_outdated(large_pix, large_path, task->fi->mtime))
        {
            /* large_pix is freed in is_thumbnail_outdated() if it's out of date. */
            /* generate large size thumbnail */
            task->flags |= GENERATE_LARGE;
            normal_pix = NULL;
        }
    }

    if( IS_CANCELLED(task) )
        goto _out;

    if(task->flags & (GENERATE_NORMAL|GENERATE_LARGE)) /* need to re-generate some thumbnails */
    {
        GList* generate_reqs = NULL, *l;
        ThumbnailTask* generate_task;

        /* all requested thumbnails need to be re-generated. */
        if( ((task->flags & LOAD_NORMAL|LOAD_LARGE) << 2) == (task->flags & (GENERATE_NORMAL|GENERATE_LARGE)) )
        {
            task->uri = g_strdup(task->uri);
            task->normal_path = g_strdup(normal_path);
            task->large_path = g_strdup(large_path);
            task->flags |= ALLOC_STRINGS;

            /* push the whole task into generator queue */
            queue_generate(task);
            return;
        }

        /* remove all requests which requires re-generating thumbnails from task and gather them in a list */
        for(l=task->requests; l; l=l->next)
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
        }

        /* this list contains requests requiring regeration of thumbnails */
        if(generate_reqs)
        {
            generate_task = g_slice_new(ThumbnailTask);
            generate_task->flags = task->flags | ALLOC_STRINGS;
            generate_task->fi = fm_file_info_ref(task->fi);
            generate_task->requests = generate_reqs;
            generate_task->uri = g_strdup(task->uri);
            generate_task->normal_path = g_strdup(task->normal_path);
            generate_task->large_path = g_strdup(task->large_path);
            DEBUG("queue regenerate for :%s", task->fi->path->name);
            /* queue the re-generation task */
            queue_generate(generate_task);
        }
    }

_out:
    G_LOCK(queue);
    g_queue_pop_head(&loader_queue); /* remove the task from the queue */
    /* thumbnails which don't require re-generation should all be loaded at this point. */
    if( IS_CANCELLED(task) )
        thumbnail_task_free(task);
    else
        thumbnail_task_finish(task, normal_pix, large_pix);
    /* task is freed in thumbnail_task_finish() */
    G_UNLOCK(queue);

    if(normal_pix)
        g_object_unref(normal_pix);
    if(large_pix)
        g_object_unref(large_pix);

    return;
}


gpointer load_thumbnail_thread(gpointer user_data)
{
    ThumbnailTask* task;
    GChecksum* sum = g_checksum_new(G_CHECKSUM_MD5);
    char* normal_path = NULL;
    int normal_prefix;
    char* large_path = NULL;
    int large_prefix;

    for(;;)
    {
        G_LOCK(queue);
        task = g_queue_peek_head(&loader_queue);
        if(G_LIKELY(task))
        {
            FmThumbnailRequest* req;
            char* uri = fm_path_to_uri(task->fi->path);
            char* thumb_path;
            const char* md5;

            G_UNLOCK(queue);

            /* generate filename for the thumbnail */
            g_checksum_update(sum, uri, -1);
            md5 = g_checksum_get_string(sum); /* md5 sum of the URI */

            task->uri = uri;
            if(task->flags & LOAD_NORMAL)
            {
                if(G_UNLIKELY(!normal_path))
                {
                    normal_path = g_build_filename(thumb_dir, "normal/00000000000000000000000000000000.png", NULL);
                    normal_prefix = strlen(g_get_home_dir()) + 20;
                    normal_path[normal_prefix -1] = '\0';
                    /* ensure thumbnail directory */
                    g_mkdir_with_parents(normal_path, 0700);
                    normal_path[normal_prefix -1] = '/';
                }
                memcpy(normal_path + normal_prefix, md5, 32);
                // DEBUG("normal_path: %s", normal_path);
                task->normal_path = normal_path;
            }
            else
                task->normal_path = NULL;

            if(task->flags & LOAD_LARGE)
            {
                if(G_UNLIKELY(!normal_path))
                {
                    large_path = g_build_filename(thumb_dir, "large/00000000000000000000000000000000.png", NULL);
                    large_prefix = strlen(g_get_home_dir()) + 19;
                    large_path[large_prefix -1] = '\0';
                    /* ensure thumbnail directory */
                    g_mkdir_with_parents(large_path, 0700);
                    large_path[large_prefix -1] = '/';
                }
                memcpy(large_path + large_prefix, md5, 32);
                // DEBUG("large_path: %s", large_path);
                task->large_path = large_path;
            }
            else
                task->large_path = NULL;

            if( !IS_CANCELLED(task) )
                load_thumbnails(task);

            g_checksum_reset(sum);
            g_free(uri);
        }
        else /* no task is left in the loader_queue */
        {
            loader_thread_id = NULL;
            G_UNLOCK(queue);
            break;
        }
    }
    g_free(normal_path);
    g_free(large_path);
    g_checksum_free(sum);
    return NULL;
}

/* called with queue locked */
ThumbnailTask* find_queued_task(GQueue* queue, FmFileInfo* fi)
{
    GList* l;
    for( l = queue->head; l; l=l->next )
    {
        ThumbnailTask* task = (ThumbnailTask*)l->data;
        if(G_UNLIKELY(task->fi == fi || fm_path_equal(task->fi->path, fi->path)))
            return task;
    }
    return NULL;
}


FmThumbnailRequest* fm_thumbnail_request(FmFileInfo* src_file,
                                    guint size,
                                    FmThumbnailReadyCallback callback,
                                    gpointer user_data)
{
    FmThumbnailRequest* req;
    ThumbnailTask* task;
    gboolean found = FALSE;
    req = g_slice_new(FmThumbnailRequest);
    req->fi = fm_file_info_ref(src_file);
    req->size = size;
    req->callback = callback;
    req->user_data = user_data;
    req->pix = NULL;

    G_LOCK(queue);
    /* FIXME: find in the cache first to see if thumbnail is already cached */
    if(found)
    {
        /* call the ready callback in main loader_thread_id from idle handler. */
        G_UNLOCK(queue);
        return req;
    }

    /* if it's not cached, add it to the loader_queue for loading. */
    task = find_queued_task(&loader_queue, src_file);

    /*
     FIXME: what to do if it's found in generator queue?
    if(!task)
        task = find_queued_task(&generator_queue, src_file);
    */

    if(!task)
    {
        task = g_slice_new0(ThumbnailTask);
        task->fi = fm_file_info_ref(src_file);
        g_queue_push_tail(&loader_queue, task);
    }

    if(size > 128)
        task->flags |= LOAD_LARGE;
    else
        task->flags |= LOAD_NORMAL;

    task->requests = g_list_append(task->requests, req);

    if(!loader_thread_id)
        loader_thread_id = g_thread_create( load_thumbnail_thread, NULL, FALSE, NULL);

    G_UNLOCK(queue);
    return req;
}

void fm_thumbnail_request_cancel(FmThumbnailRequest* req)
{
    ThumbnailTask* task;
    GList* l, *l2;

    G_LOCK(queue);
    /* if it's in generator queue (most likely) */
    for(l=generator_queue.head; l; l=l->next)
    {
        task = (ThumbnailTask*)l->data;
        if(l2 = g_list_find(task->requests, req)) /* found the request */
        {
            task->requests = g_list_delete_link(task->requests, l2);
            if(!task->requests) /* no one is requesting this thumbnail */
            {
                if(l == generator_queue.head) /* this is the currently processed item */
                {
                    task->flags |= CANCEL;
                    if(generator_cancellable)
                        g_cancellable_cancel(generator_cancellable);
                    g_queue_delete_link(&generator_queue, l);
                }
                else
                {
                    g_queue_delete_link(&generator_queue, l);
                    thumbnail_task_free(task);
                }
            }
            G_UNLOCK(queue);
            return;
        }
    }

    /* not found, try loader queue */
    for(l=loader_queue.head; l; l=l->next)
    {
        task = (ThumbnailTask*)l->data;
        if(l2 = g_list_find(task->requests, req)) /* found the request */
        {
            task->requests = g_list_delete_link(task->requests, l2);
            if(!task->requests) /* no one is requesting this thumbnail */
            {
                if(l == loader_queue.head) /* this is the currently processed item */
                {
                    task->flags |= CANCEL;
                    g_queue_delete_link(&generator_queue, l);
                }
                else
                {
                    g_queue_delete_link(&loader_queue, l);
                    thumbnail_task_free(task);
                }
            }
            G_UNLOCK(queue);
            return;
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
    G_UNLOCK(queue);
}

GdkPixbuf* fm_thumbnail_request_get_pixbuf(FmThumbnailRequest* req)
{
    return req->pix;
}

FmFileInfo* fm_thumbnail_request_get_file_info(FmThumbnailRequest* req)
{
    return req->fi;
}

guint fm_thumbnail_request_get_size(FmThumbnailRequest* req)
{
    return req->size;
}

void fm_thumbnail_init()
{
    thumb_dir = g_build_filename(g_get_home_dir(), ".thumbnails", NULL);
}

void fm_thumbnail_finalize()
{
    /* FIXME: cancel all pending requests... */
    g_free(thumb_dir);
}

gpointer generate_thumbnail_thread(gpointer user_data)
{
    ThumbnailTask* task;
    generator_cancellable = g_cancellable_new();
    for(;;)
    {
        G_LOCK(queue);
        task = g_queue_peek_head(&generator_queue);
        DEBUG("peek task from generator queue: %p", task);

        if( G_LIKELY(task) )
        {
            G_UNLOCK(queue);

            if(fm_file_info_is_image(task->fi))
                generate_thumbnails_with_gdk_pixbuf(task);
            else
                generate_thumbnails_with_thumbnailers(task);

            if(g_cancellable_is_cancelled(generator_cancellable))
            {
                DEBUG("generation of thumbnail is cancelled!")
                g_cancellable_reset(generator_cancellable);
            }
        }
        else
        {
            generator_thread_id = NULL;
            DEBUG("no task is in generator queue, exit generator thread");
            g_object_unref(generator_cancellable);
            generator_cancellable = NULL;
            G_UNLOCK(queue);
            return NULL;
        }
    }
    generator_cancellable = NULL;
    G_UNLOCK(queue);
    return NULL;
}

void queue_generate(ThumbnailTask* regenerate_task)
{
    ThumbnailTask* task;
    G_LOCK(queue);
    task = find_queued_task(&generator_queue, regenerate_task->fi);
    if(task)
    {
        task->flags |= regenerate_task->flags;
        task->requests = g_list_concat(task->requests, regenerate_task->requests);
        regenerate_task->requests = NULL;
        thumbnail_task_free(task);

        G_UNLOCK(queue);
        return;
    }
    DEBUG("push into generator queue");
    g_queue_push_tail(&generator_queue, regenerate_task);

    if(!generator_thread_id)
        generator_thread_id = g_thread_create(generate_thumbnail_thread, NULL, FALSE, NULL);

    G_UNLOCK(queue);
}

GdkPixbuf* scale_pix(GdkPixbuf* ori_pix, int size)
{
    GdkPixbuf* scaled_pix;
    /* keep aspect ratio and scale to thumbnail size: 128 or 256 */
    int width = gdk_pixbuf_get_width(ori_pix);
    int height = gdk_pixbuf_get_height(ori_pix);

    if(width > height)
    {
        gdouble aspect = (gdouble)height / width;
        width = size;
        height = size * aspect;
    }
    else if(width < height)
    {
        gdouble aspect = (gdouble)width / height;
        height = size;
        width = size * aspect;
    }
    else
    {
        width = height = size;
    }
    if(width != size || height != size)
        scaled_pix = gdk_pixbuf_scale_simple(ori_pix, width, height, GDK_INTERP_BILINEAR);
    else
        scaled_pix = (GdkPixbuf*)g_object_ref(ori_pix);

    return scaled_pix;
}

void save_thumbnail_to_disk(ThumbnailTask* task, GdkPixbuf* pix, const char* path)
{
    /* save the generated thumbnail to disk */
    char* tmpfile = g_strconcat(path, ".XXXXXX", NULL);
    gint fd;
    fd = g_mkstemp(tmpfile); /* save to a temp file first */
    if(fd != -1)
    {
        char mtime_str[100];
        g_snprintf( mtime_str, 100, "%lu", task->fi->mtime );
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

void generate_thumbnails_with_gdk_pixbuf(ThumbnailTask* task)
{
    /* FIXME: only formats supported by GdkPixbuf should be handled this way. */
    GFile* gf = fm_path_to_gfile(task->fi->path);
    GFileInputStream* ins;
    GdkPixbuf* normal_pix = NULL;
    GdkPixbuf* large_pix = NULL;

    DEBUG("generate thumbnail for %s", task->fi->path->name);

    if( ins = g_file_read(gf, generator_cancellable, NULL) )
    {
        GdkPixbuf* ori_pix;
        GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
        char buf[4096];
        gssize len;

        /* read the image file */
        while( (len = g_input_stream_read(ins, buf, sizeof(buf), generator_cancellable, NULL)) > 0)
            gdk_pixbuf_loader_write(loader, buf, len, NULL);
        gdk_pixbuf_loader_close(loader, NULL);
        g_input_stream_close(ins, NULL, NULL);
        g_object_unref(ins);

        if(len != -1) /* no error */
        {
            ori_pix = gdk_pixbuf_loader_get_pixbuf(loader);
            if(ori_pix)
                ori_pix = (GdkPixbuf*)g_object_ref(ori_pix);
        }
        else
            ori_pix = NULL;
        g_object_unref(loader); /* free the pixbuf loader and retains the loaded image */

        if(ori_pix) /* if the original image is successfully loaded */
        {
            int width = gdk_pixbuf_get_width(ori_pix);
            int height = gdk_pixbuf_get_height(ori_pix);

            if(task->flags & GENERATE_NORMAL)
            {
                /* don't create thumbnails for images which are too small */
                if(width <=128 && height <= 128)
                    normal_pix = (GdkPixbuf*)g_object_ref(ori_pix);
                else
                {
                    normal_pix = scale_pix(ori_pix, 128);
                    save_thumbnail_to_disk(task, normal_pix, task->normal_path);
                }
            }

            if(task->flags & GENERATE_LARGE)
            {
                /* don't create thumbnails for images which are too small */
                if(width <=256 && height <= 256)
                    large_pix = (GdkPixbuf*)g_object_ref(ori_pix);
                else
                {
                    large_pix = scale_pix(ori_pix, 256);
                    save_thumbnail_to_disk(task, large_pix, task->large_path);
                }
            }
            g_object_unref(ori_pix);
        }
    }

    G_LOCK(queue);
    DEBUG("remove generator task from queue: %p", task);
    g_queue_pop_head(&generator_queue); /* really remove the task from the queue */
    thumbnail_task_finish(task, normal_pix, large_pix);
    G_UNLOCK(queue);

    if(normal_pix)
        g_object_unref(normal_pix);
    if(large_pix)
        g_object_unref(large_pix);

    g_object_unref(gf);
}

void generate_thumbnails_with_thumbnailers(ThumbnailTask* task)
{
    /* TODO: external thumbnailer support */
    DEBUG("external thumbnailer is needed for %s", task->fi->disp_name);

    G_LOCK(queue);
    DEBUG("remove generator task from queue: %p", task);
    g_queue_pop_head(&generator_queue); /* really remove the task from the queue */
    thumbnail_task_finish(task, NULL, NULL);
    G_UNLOCK(queue);
}
