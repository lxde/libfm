/*
 *      fm-xml-parser.c
 *
 *      Copyright 2013 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

/**
 * SECTION:fm-xml-parser
 * @short_description: Simple XML parser.
 * @title: FmXmlParser
 *
 * @include: libfm/fm.h
 *
 * The FmXmlParser represents content of some XML file in form that can
 * be altered and saved later.
 *
 * This parser has some simplifications on XML parsing:
 * * Only UTF-8 encoding is supported
 * * No user-defined entities, those should be converted externally
 * * Processing instructions, comments and the doctype declaration are parsed but are not interpreted in any way
 * The markup format does support:
 * * Elements
 * * Attributes
 * * 5 standard entities: &amp;amp; &amp;lt; &amp;gt; &amp;quot; &amp;apos;
 * * Character references
 * * Sections marked as CDATA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "glib-compat.h"

#include "fm-xml-parser.h"

#include <stdlib.h>
#include <errno.h>

typedef struct
{
    gchar *name;
    FmXmlParserHandler handler;
} FmXmlParserTagDesc;

struct _FmXmlParser
{
    GObject parent;
    GList *items;
    GString *data;
    char *comment_pre;
    FmXmlParserItem *current_item;
    FmXmlParserTagDesc *tags; /* tags[0].name contains DTD */
    guint n_tags; /* number of elements in tags */
    guint line, pos;
};

struct _FmXmlParserClass
{
    GObjectClass parent_class;
};

struct _FmXmlParserItem
{
    FmXmlParserTag tag;
    union {
        gchar *tag_name; /* only for tag == FM_XML_PARSER_TAG_NOT_HANDLED */
        gchar *text; /* only for tag == FM_XML_PARSER_TEXT, NULL if directive */
    };
    char **attribute_names;
    char **attribute_values;
    FmXmlParser *parser;
    FmXmlParserItem *parent;
    GList **parent_list; /* points to parser->items or to parent->children */
    GList *children;
    gchar *comment; /* a little trick: it is equal to text if it is CDATA */
};


G_DEFINE_TYPE(FmXmlParser, _fm_xml_parser, G_TYPE_OBJECT);

static void _fm_xml_parser_finalize(GObject *object)
{
    FmXmlParser *self;
    guint i;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_XML_PARSER(object));

    self = (FmXmlParser*)object;
    while (self->items)
        fm_xml_parser_item_destroy(self->items->data);
    for (i = 0; i < self->n_tags; i++)
        g_free(self->tags[i].name);
    g_free(self->tags);
    if (self->data)
        g_string_free(self->data, TRUE);
    g_free(self->comment_pre);

    G_OBJECT_CLASS(_fm_xml_parser_parent_class)->finalize(object);
}

static void _fm_xml_parser_class_init(FmXmlParserClass *klass)
{
    GObjectClass *g_object_class;

    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = _fm_xml_parser_finalize;
}

static void _fm_xml_parser_init(FmXmlParser *self)
{
    self->tags = g_new0(FmXmlParserTagDesc, 1);
    self->n_tags = 1;
    self->line = 1;
}

/**
 * fm_xml_parser_new
 * @sibling: (allow-none): container to copy handlers data
 *
 * Creates new empty #FmXmlParser container. If @sibling is not %NULL
 * then new container will have callbacks identical to set in @sibling.
 * Use @sibling parameter if you need to work with few XML files that
 * share the same schema or if you need to use the same tag ids for more
 * than one parser.
 *
 * Returns: (transfer full): newly created object.
 *
 * Since: 1.2.0
 */
FmXmlParser *fm_xml_parser_new(FmXmlParser *sibling)
{
    FmXmlParser *self;
    FmXmlParserTag i;

    self = (FmXmlParser*)g_object_new(FM_XML_PARSER_TYPE, NULL);
    if (sibling && sibling->n_tags > 1)
    {
        self->n_tags = sibling->n_tags;
        self->tags = g_renew(FmXmlParserTagDesc, self->tags, self->n_tags);
        for (i = 1; i < self->n_tags; i++)
        {
            self->tags[i].name = g_strdup(sibling->tags[i].name);
            self->tags[i].handler = sibling->tags[i].handler;
        }
    }
    return self;
}

/**
 * fm_xml_parser_set_handler
 * @parser: the parser container
 * @tag: tag to use @handler for
 * @handler: callback for @tag
 * @error: (allow-none) (out): location to save error
 *
 * Sets @handler for @parser to be called on parse when @tag is found
 * in XML data.
 *
 * Returns: id for the @tag.
 *
 * Since: 1.2.0
 */
FmXmlParserTag fm_xml_parser_set_handler(FmXmlParser *parser, const char *tag,
                                         FmXmlParserHandler handler,
                                         GError **error)
{
    FmXmlParserTag i;

    g_return_val_if_fail(parser != NULL && FM_IS_XML_PARSER(parser), FM_XML_PARSER_TAG_NOT_HANDLED);
    g_return_val_if_fail(handler != NULL, FM_XML_PARSER_TAG_NOT_HANDLED);
    g_return_val_if_fail(tag != NULL, FM_XML_PARSER_TAG_NOT_HANDLED);
    for (i = 1; i < parser->n_tags; i++)
        if (strcmp(parser->tags[i].name, tag) == 0)
        {
            g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                        _("Duplicate handler for tag <%s>"), tag);
            return i;
        }
    parser->tags = g_renew(FmXmlParserTagDesc, parser->tags, i + 1);
    parser->tags[i].name = g_strdup(tag);
    parser->tags[i].handler = handler;
    return i;
}


/* parse */

/*
 * This function was taken from GLib sources and adapted to be used here.
 * Copyright 2000, 2003 Red Hat, Inc.
 *
 * re-write the GString in-place, unescaping anything that escaped.
 * most XML does not contain entities, or escaping.
 */
static gboolean
unescape_gstring_inplace (//GMarkupParseContext  *context,
                          GString              *string,
                          //gboolean             *is_ascii,
                          guint                *line_num,
                          guint                *pos,
                          gboolean              normalize_attribute,
                          GError              **error)
{
  //char mask, *to;
  char *to;
  //int line_num = 1;
  const char *from, *sol;

  //*is_ascii = FALSE;

  /*
   * Meeks' theorum: unescaping can only shrink text.
   * for &lt; etc. this is obvious, for &#xffff; more
   * thought is required, but this is patently so.
   */
  //mask = 0;
  for (from = to = string->str; *from != '\0'; from++, to++)
    {
      *to = *from;

      //mask |= *to;
      if (*to == '\n')
      {
        (*line_num)++;
        *pos = 0;
      }
      if (normalize_attribute && (*to == '\t' || *to == '\n'))
        *to = ' ';
      if (*to == '\r')
        {
          *to = normalize_attribute ? ' ' : '\n';
          if (from[1] == '\n')
          {
            from++;
            (*line_num)++;
            *pos = 0;
          }
        }
      sol = from;
      if (*from == '&')
        {
          from++;
          if (*from == '#')
            {
              gboolean is_hex = FALSE;
              gulong l;
              gchar *end = NULL;

              from++;

              if (*from == 'x')
                {
                  is_hex = TRUE;
                  from++;
                }

              /* digit is between start and p */
              errno = 0;
              if (is_hex)
                l = strtoul (from, &end, 16);
              else
                l = strtoul (from, &end, 10);

              if (end == from || errno != 0)
                {
                  g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                              _("Failed to parse '%-.*s', which "
                                "should have been a digit "
                                "inside a character reference "
                                "(&#234; for example) - perhaps "
                                "the digit is too large"),
                              end - from, from);
                  return FALSE;
                }
              else if (*end != ';')
                {
                  g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                              _("Character reference did not end with a "
                                "semicolon; "
                                "most likely you used an ampersand "
                                "character without intending to start "
                                "an entity - escape ampersand as &amp;"));
                  return FALSE;
                }
              else
                {
                  /* characters XML 1.1 permits */
                  if ((0 < l && l <= 0xD7FF) ||
                      (0xE000 <= l && l <= 0xFFFD) ||
                      (0x10000 <= l && l <= 0x10FFFF))
                    {
                      gchar buf[8];
                      memset (buf, 0, 8);
                      g_unichar_to_utf8 (l, buf);
                      strcpy (to, buf);
                      to += strlen (buf) - 1;
                      from = end;
                      //if (l >= 0x80) /* not ascii */
                        //mask |= 0x80;
                    }
                  else
                    {
                      g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                  _("Character reference '%-.*s' does not "
                                    "encode a permitted character"),
                                  end - from, from);
                      return FALSE;
                    }
                }
            }

          else if (strncmp (from, "lt;", 3) == 0)
            {
              *to = '<';
              from += 2;
            }
          else if (strncmp (from, "gt;", 3) == 0)
            {
              *to = '>';
              from += 2;
            }
          else if (strncmp (from, "amp;", 4) == 0)
            {
              *to = '&';
              from += 3;
            }
          else if (strncmp (from, "quot;", 5) == 0)
            {
              *to = '"';
              from += 4;
            }
          else if (strncmp (from, "apos;", 5) == 0)
            {
              *to = '\'';
              from += 4;
            }
          else
            {
              if (*from == ';')
                g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                            _("Empty entity '&;' seen; valid "
                              "entities are: &amp; &quot; &lt; &gt; &apos;"));
              else
                {
                  const char *end = strchr (from, ';');
                  if (end)
                    g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                _("Entity name '%-.*s' is not known"),
                                end-from, from);
                  else
                    g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                _("Entity did not end with a semicolon; "
                                  "most likely you used an ampersand "
                                  "character without intending to start "
                                  "an entity - escape ampersand as &amp;"));
                }
              return FALSE;
            }
        }
      *pos += (from - sol) + 1;
    }

  g_assert (to - string->str <= (gint)string->len);
  if (to - string->str != (gint)string->len)
    g_string_truncate (string, to - string->str);

  //*is_ascii = !(mask & 0x80);

  return TRUE;
}


static inline void _update_file_ptr_part(FmXmlParser *parser, const char *start,
                                         const char *end)
{
    while (start < end)
    {
        if (*start == '\n')
        {
            parser->line++;
            parser->pos = 0;
        }
        else
            /* FIXME: advance by chars not bytes? */
            parser->pos++;
        start++;
    }
}

static inline void _update_file_ptr(FmXmlParser *parser, int add_cols)
{
    guint i;
    char *p;

    for (i = parser->data->len, p = parser->data->str; i > 0; i--, p++)
    {
        if (*p == '\n')
        {
            parser->line++;
            parser->pos = 0;
        }
        else
            /* FIXME: advance by chars not bytes? */
            parser->pos++;
    }
    parser->pos += add_cols;
}

static inline gboolean _is_space(char c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

/**
 * fm_xml_parser_parse_data
 * @parser: the parser container
 * @text: data to parse
 * @size: size of @text
 * @error: (allow-none) (out): location to save error
 * @user_data: data to pass to handlers
 *
 * Parses next chunk of @text data. Parsing stops at end of data or at any
 * error. In latter case @error will be set appropriately.
 *
 * See also: fm_xml_parser_finish().
 *
 * Returns: %FALSE if parsing failed.
 *
 * Since: 1.2.0
 */
gboolean fm_xml_parser_parse_data(FmXmlParser *parser, const char *text,
                                  gsize size, GError **error, gpointer user_data)
{
    gsize ptr, len;
    char *dst, *end, *tag, *name, *value;
    GString *buff;
    FmXmlParserItem *item;
    gboolean closing, selfdo;
    FmXmlParserTag i;
    char **attrib_names, **attrib_values;
    guint attribs;
    char quote;

    g_return_val_if_fail(parser != NULL && FM_IS_XML_PARSER(parser), FALSE);
_restart:
    if (size == 0)
        return TRUE;
    /* if parser->data has '<' as first char then we stopped at tag */
    if (parser->data && parser->data->len && parser->data->str[0] == '<')
    {
        for (ptr = 0; ptr < size; ptr++)
            if (text[ptr] == '>')
                break;
        if (ptr == size) /* still no end of that tag */
        {
            g_string_append_len(parser->data, text, size);
            return TRUE;
        }
        /* we got a complete tag, nice, let parse it */
        g_string_append_len(parser->data, text, ptr);
        ptr++;
        text += ptr;
        size -= ptr;
        /* check for CDATA first */
        if (parser->data->len >= 11 /* <![CDATA[]] */ &&
            strncmp(parser->data->str, "<![CDATA[", 9) == 0)
        {
            end = parser->data->str + parser->data->len;
            if (end[-2] != ']' || end[-1] != ']') /* find end of CDATA */
            {
                g_string_append_c(parser->data, '>');
                goto _restart;
            }
            if (parser->current_item == NULL) /* CDATA at top level! */
                g_warning("FmXmlParser: line %u: junk CDATA in XML file ignored",
                          parser->line);
            else
            {
                item = fm_xml_parser_item_new(FM_XML_PARSER_TEXT);
                item->text = item->comment = g_strndup(&parser->data->str[9],
                                                       parser->data->len - 11);
                fm_xml_parser_item_append_child(parser->current_item, item);
            }
            _update_file_ptr(parser, 1);
            g_string_truncate(parser->data, 0);
            goto _restart;
        }
        /* check for comment */
        if (parser->data->len >= 7 /* <!-- -- */ &&
            strncmp(parser->data->str, "<!--", 4) == 0 &&
            _is_space(parser->data->str[4]))
        {
            end = parser->data->str + parser->data->len;
            if (end[-2] != '-' || end[-1] != '-') /* find end of comment */
            {
                g_string_append_c(parser->data, '>');
                goto _restart;
            }
            g_free(parser->comment_pre);
            /* FIXME: not ignore duplicate comments */
            if (_is_space(end[-3]))
                parser->comment_pre = g_strndup(&parser->data->str[5],
                                                parser->data->len - 8);
            else /* FIXME: check: XML spec says it should be not '-' */
                parser->comment_pre = g_strndup(&parser->data->str[5],
                                                parser->data->len - 7);
            _update_file_ptr(parser, 1);
            g_string_truncate(parser->data, 0);
            goto _restart;
        }
        /* check for DTD - it may be only at top level */
        if (parser->current_item == NULL && parser->data->len >= 10 &&
            strncmp(parser->data->str, "<!DOCTYPE", 9) == 0 &&
            _is_space(parser->data->str[9]))
        {
            /* FIXME: can DTD contain any tags? count '<' and '>' pairs */
            if (parser->tags[0].name) /* duplicate DTD! */
                g_warning("FmXmlParser: line %u: duplicate DTD, ignored",
                          parser->line);
            else
                parser->tags[0].name = g_strndup(&parser->data->str[10],
                                                  parser->data->len - 10);
            _update_file_ptr(parser, 1);
            g_string_truncate(parser->data, 0);
            goto _restart;
        }
        /* support directives such as <?xml ..... ?> */
        if (parser->data->len >= 4 /* <?x? */ &&
            parser->data->str[1] == '?' &&
            parser->data->str[parser->data->len-1] == '?')
        {
            item = fm_xml_parser_item_new(FM_XML_PARSER_TEXT);
            item->comment = g_strndup(&parser->data->str[2], parser->data->len - 3);
            if (parser->current_item != NULL)
                fm_xml_parser_item_append_child(parser->current_item, item);
            else
            {
                item->parser = parser;
                item->parent_list = &parser->items;
                parser->items = g_list_append(parser->items, item);
            }
            _update_file_ptr(parser, 1);
            g_string_truncate(parser->data, 0);
            goto _restart;
        }
        closing = (parser->data->str[1] == '/');
        end = parser->data->str + parser->data->len;
        selfdo = (!closing && end[-1] == '/');
        if (selfdo)
            end--;
        tag = closing ? &parser->data->str[2] : &parser->data->str[1];
        for (dst = tag; dst < end; dst++)
            if (_is_space(*dst))
                break;
        _update_file_ptr_part(parser, parser->data->str, dst + 1);
        *dst = '\0'; /* terminate the tag */
        if (closing)
        {
            if (dst != end) /* we got a space char in closing tag */
            {
                g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                    _("Space isn't allowed in the close tag"));
                return FALSE;
            }
            item = parser->current_item;
            if (item == NULL) /* no tag to close */
            {
                g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                            _("Element '%s' was closed but no element was opened"),
                            tag);
                return FALSE;
            }
            else
            {
                char *tagname;

                if (item->tag == FM_XML_PARSER_TAG_NOT_HANDLED)
                    tagname = item->tag_name;
                else
                    tagname = parser->tags[item->tag].name;
                if (strcmp(tag, tagname)) /* closing tag doesn't match */
                {
                    /* FIXME: validate tag so be more verbose on error */
                    g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                _("Element '%s' was closed but the currently "
                                  "open element is '%s'"), tag, tagname);
                    return FALSE;
                }
                parser->current_item = item->parent;
_close_the_tag:
                g_string_truncate(parser->data, 0);
                if (item->tag != FM_XML_PARSER_TAG_NOT_HANDLED)
                {
                    if (!parser->tags[item->tag].handler(item, item->children,
                                                         item->attribute_names,
                                                         item->attribute_values,
                                                         item->attribute_names ? g_strv_length(item->attribute_names) : 0,
                                                         parser->line,
                                                         parser->pos,
                                                         error, user_data))
                        return FALSE;
                }
                parser->pos++; /* '>' */
                goto _restart;
            }
        }
        else /* opening tag */
        {
            /* parse and check tag name */
            for (i = 1; i < parser->n_tags; i++)
                if (strcmp(parser->tags[i].name, tag) == 0)
                    break;
            if (i == parser->n_tags)
                /* FIXME: do name validation */
                i = FM_XML_PARSER_TAG_NOT_HANDLED;
            /* parse and check attributes */
            attribs = 0;
            attrib_names = attrib_values = NULL;
            while (dst < end)
            {
                name = &dst[1]; /* skip this space */
                while (name < end && _is_space(*name))
                    name++;
                value = name;
                while (value < end && !_is_space(*value) && *value != '=')
                    value++;
                len = value - name;
                _update_file_ptr_part(parser, dst, value);
                /* FIXME: skip spaces before =? */
                if (value + 3 <= end && *value == '=') /* minimum is ="" */
                {
                    value++;
                    parser->pos++; /* '=' */
                    /* FIXME: skip spaces after =? */
                    quote = *value++;
                    if (quote != '\'' && quote != '"')
                    {
                        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                    _("Invalid char '%c' at start of attribute value"),
                                    quote);
                        goto _attr_error;
                    }
                    parser->pos++; /* quote char */
                    for (ptr = 0; &value[ptr] < end; ptr++)
                        if (value[ptr] == quote)
                            break;
                    if (&value[ptr] == end)
                    {
                        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                    _("Invalid char '%c' at end of attribute value,"
                                      " expected '%c'"), value[ptr-1], quote);
                        goto _attr_error;
                    }
                    buff = g_string_new_len(value, ptr);
                    if (unescape_gstring_inplace(buff, &parser->line,
                                                 &parser->pos, TRUE, error))
                    {
                        g_string_free(buff, TRUE);
_attr_error:
                        for (i = 0; i < attribs; i++)
                        {
                            g_free(attrib_names[i]);
                            g_free(attrib_values[i]);
                        }
                        g_free(attrib_names);
                        g_free(attrib_values);
                        return FALSE;
                    }
                    dst = &value[ptr+1];
                    value = g_string_free(buff, FALSE);
                    parser->pos++; /* end quote char */
                }
                else
                {
                    dst = value;
                    value = NULL;
                    /* FIXME: isn't it error? */
                }
                attrib_names = g_renew(char *, attrib_names, attribs + 2);
                attrib_values = g_renew(char *, attrib_values, attribs + 2);
                attrib_names[attribs] = g_strndup(name, len);
                attrib_values[attribs] = value;
                attribs++;
            }
            attrib_names[attribs] = NULL;
            attrib_values[attribs] = NULL;
            /* create new item */
            item = fm_xml_parser_item_new(i);
            item->attribute_names = attrib_names;
            item->attribute_values = attrib_values;
            if (i == FM_XML_PARSER_TAG_NOT_HANDLED)
                item->tag_name = g_strdup(tag);
            /* insert new item into the container */
            item->comment = parser->comment_pre;
            parser->comment_pre = NULL;
            if (parser->current_item)
                fm_xml_parser_item_append_child(parser->current_item, item);
            else
            {
                item->parser = parser;
                item->parent_list = &parser->items;
                parser->items = g_list_append(parser->items, item);
            }
            parser->pos++; /* '>' or '/' */
            if (selfdo) /* simple self-closing tag */
                goto _close_the_tag;
            parser->current_item = item;
            g_string_truncate(parser->data, 0);
            goto _restart;
        }
    }
    /* otherwise we stopped at some data somewhere */
    else
    {
        if (!parser->data || parser->data->len == 0) while (size > 0)
        {
            /* skip leading spaces */
            if (*text == '\n')
            {
                parser->line++;
                parser->pos = 0;
            }
            else if (*text == ' ' || *text == '\t' || *text == '\r')
                parser->pos++;
            else
                break;
            text++;
            size--;
        }
        for (ptr = 0; ptr < size; ptr++)
            if (text[ptr] == '<')
                break;
        if (parser->data == NULL)
            parser->data = g_string_new_len(text, ptr);
        else if (ptr > 0)
            g_string_append_len(parser->data, text, ptr);
        if (ptr == size) /* still no end of text */
            return TRUE;
        if (parser->current_item == NULL) /* text at top level! */
        {
            g_warning("FmXmlParser: line %u: junk data in XML file ignored",
                      parser->line);
            _update_file_ptr(parser, 0);
        }
        else if (unescape_gstring_inplace(parser->data, &parser->line,
                                          &parser->pos, FALSE, error))
        {
            item = fm_xml_parser_item_new(FM_XML_PARSER_TEXT);
            item->text = g_strndup(parser->data->str, parser->data->len);
            item->comment = parser->comment_pre;
            parser->comment_pre = NULL;
            fm_xml_parser_item_append_child(parser->current_item, item);
            /* FIXME: truncate ending spaces from item->text */
        }
        else
            return FALSE;
        ptr++;
        text += ptr;
        size -= ptr;
        g_string_assign(parser->data, "<");
        goto _restart;
    }
    /* error if reached */
}

/**
 * fm_xml_parser_finish
 * @parser: the parser container
 * @error: (allow-none) (out): location to save error
 *
 * Ends parsing of data and retrieves final status. If XML was invalid
 * then returns %NULL and sets @error appropriately.
 *
 * See also: fm_xml_parser_parse_data().
 *
 * Returns: (transfer container) (element-type FmXmlParserItem): contents of XML
 *
 * Since: 1.2.0
 */
GList *fm_xml_parser_finish(FmXmlParser *parser, GError **error)
{
    g_return_val_if_fail(parser != NULL && FM_IS_XML_PARSER(parser), NULL);
    if (parser->current_item)
    {
        if (parser->current_item->tag == FM_XML_PARSER_TEXT &&
            parser->current_item->parent_list == &parser->items)
            g_warning("FmXmlParser: junk at end of XML");
        else
        {
            g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                _("Document ended unexpectedly"));
            /* FIXME: analize content of parser->data to be more verbose */
            return NULL;
        }
    }
    else if (parser->items == NULL)
    {
        g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY,
                            _("Document was empty or contained only whitespace"));
        return NULL;
    }
    /* FIXME: check if parser->comment_pre is NULL */
    return g_list_copy(parser->items);
}

/**
 * fm_xml_parser_get_dtd
 * @parser: the parser container
 *
 * Retrieves DTD description for XML data in the container. Returned data
 * are owned by @parser and should not be modified by caller.
 *
 * Returns: (transfer none): DTD description.
 *
 * Since: 1.2.0
 */
const char *fm_xml_parser_get_dtd(FmXmlParser *parser)
{
    if(parser == NULL)
        return NULL;
    return parser->tags[0].name;
}


/* item manipulations */

/**
 * fm_xml_parser_item_new
 * @tag: tag id for new item
 *
 * Creates new unattached XML item.
 *
 * Returns: (transfer full): newly allocated #FmXmlParserItem.
 *
 * Since: 1.2.0
 */
FmXmlParserItem *fm_xml_parser_item_new(FmXmlParserTag tag)
{
    FmXmlParserItem *item = g_slice_new0(FmXmlParserItem);

    item->tag = tag;
    return item;
}

/**
 * fm_xml_parser_item_append_text
 * @item: item to append text
 * @text: text to append
 * @text_size: length of text in bytes, or -1 if the text is nul-terminated
 * @cdata: %TRUE if @text should be saved as CDATA array
 *
 * Appends @text after last element contained in @item.
 *
 * Since: 1.2.0
 */
void fm_xml_parser_item_append_text(FmXmlParserItem *item, const char *text,
                                    gssize text_size, gboolean cdata)
{
    FmXmlParserItem *text_item;

    g_return_if_fail(item != NULL);
    if (text == NULL || text_size == 0)
        return;
    text_item = fm_xml_parser_item_new(FM_XML_PARSER_TEXT);
    if (text_size > 0)
        item->text = g_strndup(text, text_size);
    else
        item->text = g_strdup(text);
    if (cdata)
        item->comment = item->text;
    fm_xml_parser_item_append_child(item, text_item);
}

/**
 * fm_xml_parser_item_append_child
 * @item: item to append child
 * @child: the child item to append
 *
 * Appends @child after last element contained in @item. If the @child
 * already was in the XML structure then it will be moved to the new
 * place instead.
 *
 * Returns: %FALSE if @child cannot be appended.
 *
 * Since: 1.2.0
 */
gboolean fm_xml_parser_item_append_child(FmXmlParserItem *item, FmXmlParserItem *child)
{
    g_return_val_if_fail(item != NULL && child != NULL, FALSE);
    if (child->parser && item->parser != child->parser)
        return FALSE; /* cannot move from one parser to another */
    if (child->parent_list) /* remove from old list */
        *child->parent_list = g_list_remove(*child->parent_list, child);
    item->children = g_list_append(item->children, child);
    child->parent_list = &item->children;
    child->parent = item;
    child->parser = item->parser;
    return TRUE;
}

/**
 * fm_xml_parser_item_set_comment
 * @item: element to set
 * @comment: (allow-none): new comment
 *
 * Changes comment that is prepended to @item.
 *
 * Since: 1.2.0
 */
void fm_xml_parser_item_set_comment(FmXmlParserItem *item, const char *comment)
{
    g_return_if_fail(item != NULL);
    g_free(item->comment);
    item->comment = g_strdup(comment);
}

#if 0
/**
 * fm_xml_parser_item_set_attribute
 * @item: element to update
 * @name: attribute name
 * @value: (allow-none): attribute data
 *
 * Changes data for the attribute of some @item with new @value. If such
 * attribute wasn't set then adds it for the @item. If @value is %NULL
 * then the attribute will be unset from the @item.
 *
 * Returns: %TRUE if attribute was set successfully.
 *
 * Since: 1.2.0
 */
gboolean fm_xml_parser_item_set_attribute(FmXmlParserItem *item,
                                          const char *name, const char *value)
{
    g_return_if_fail(item != NULL);
    
}
#endif

/**
 * fm_xml_parser_item_destroy
 * @item: element to destroy
 *
 * Removes element and its children from its parent, and frees all
 * data.
 *
 * Returns: %FALSE if @item is busy thus cannot be destroyed.
 *
 * Since: 1.2.0
 */
gboolean fm_xml_parser_item_destroy(FmXmlParserItem *item)
{
    register FmXmlParserItem *x;

    g_return_val_if_fail(item != NULL, FALSE);
    if (item->parser && item->parser->current_item)
        for (x = item; x; x = x->parent)
            if (x == item->parser->current_item)
                return FALSE;
    while (item->children)
        if (!fm_xml_parser_item_destroy(item->children->data))
            return FALSE; /* FIXME: how to diagnose this early? */
    if (item->parent_list)
        *item->parent_list = g_list_remove(*item->parent_list, item);
    g_free(item->text);
    if (item->text != item->comment)
        g_free(item->comment);
    g_strfreev(item->attribute_names);
    g_strfreev(item->attribute_values);
    g_slice_free(FmXmlParserItem, item);
    return TRUE;
}

/**
 * fm_xml_parser_insert_before
 * @item: item to insert before it
 * @new_item: new item to insert
 *
 * Inserts @new_item before @item that is already in XML structure. If
 * @new_item is already in the XML structure then it will be moved to
 * the new place instead.
 *
 * Returns: %TRUE in case of success.
 *
 * Since: 1.2.0
 */
gboolean fm_xml_parser_insert_before(FmXmlParserItem *item, FmXmlParserItem *new_item)
{
    GList *sibling;

    g_return_val_if_fail(item != NULL && new_item != NULL, FALSE);
    sibling = g_list_find(*item->parent_list, item);
    if (sibling == NULL) /* no such item found */
        return FALSE;
    if (new_item->parser && item->parser != new_item->parser)
        return FALSE; /* cannot move from one parser to another */
    if (new_item->parent_list) /* remove from old list */
        *new_item->parent_list = g_list_remove(*new_item->parent_list, new_item);
    *item->parent_list = g_list_insert_before(*item->parent_list, sibling, new_item);
    new_item->parent_list = item->parent_list;
    new_item->parent = item->parent;
    new_item->parser = item->parser;
    return TRUE;
}

/**
 * fm_xml_parser_insert_first
 * @parser: the parser container
 * @new_item: new item to insert
 *
 * Inserts @new_item as very first element of XML data in container.
 *
 * Returns: %TRUE in case of success.
 *
 * Since: 1.2.0
 */
gboolean fm_xml_parser_insert_first(FmXmlParser *parser, FmXmlParserItem *new_item)
{
    g_return_val_if_fail(parser != NULL && FM_IS_XML_PARSER(parser), FALSE);
    g_return_val_if_fail(new_item != NULL, FALSE);
    if (new_item->parser && parser != new_item->parser)
        return FALSE; /* cannot move from one parser to another */
    if (new_item->parent_list)
        *new_item->parent_list = g_list_remove(*new_item->parent_list, new_item);
    parser->items = g_list_prepend(parser->items, new_item);
    new_item->parent_list = &parser->items;
    new_item->parent = NULL;
    new_item->parser = parser;
    return TRUE;
}


/* save XML */

/**
 * fm_xml_parser_set_dtd
 * @parser: the parser container
 * @dtd: DTD description for XML data
 * @error: (allow-none) (out): location to save error
 *
 * Changes DTD description for XML data in the container.
 *
 * Since: 1.2.0
 */
void fm_xml_parser_set_dtd(FmXmlParser *parser, const char *dtd, GError **error)
{
    if(parser == NULL)
        return;
    /* FIXME: validate dtd */
    g_free(parser->tags[0].name);
    parser->tags[0].name = g_strdup(dtd);
}

static gboolean _parser_item_to_gstring(FmXmlParser *parser, GString *string,
                                        FmXmlParserItem *item, GString *prefix,
                                        gboolean *has_nl, GError **error)
{
    const char *tag_name;
    GList *l;

    /* open the tag */
    switch (item->tag)
    {
    case FM_XML_PARSER_TAG_NOT_HANDLED:
        if (item->tag_name == NULL)
            goto _no_tag;
        tag_name = item->tag_name;
        goto _do_tag;
    case FM_XML_PARSER_TEXT:
        if (item->text == item->comment) /* CDATA */
            g_string_append_printf(string, "<![CDATA[%s]]>", item->text);
        else if (item->text) /* just text */
        {
            char *escaped;

            if (item->comment != NULL)
                g_string_append_printf(string, "<!-- %s -->", item->comment);
            escaped = g_markup_escape_text(item->text, -1);
            g_string_append(string, escaped);
            g_free(escaped);
        }
        else /* processing directive */
        {
            g_string_append_printf(string, "%s<?%s?>", prefix->str, item->comment);
            *has_nl = TRUE;
        }
        return TRUE;
    default:
        if (item->tag >= parser->n_tags)
        {
_no_tag:
            g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                                _("fm_xml_parser_to_data: XML data error"));
            return FALSE;
        }
        tag_name = parser->tags[item->tag].name;
_do_tag:
        /* do comment */
        if (item->comment != NULL)
            g_string_append_printf(string, "%s<!-- %s -->", prefix->str,
                                   item->comment);
        else if (item->attribute_names == NULL && item->children == NULL)
        {
            /* don't add prefix if it is simple tag such as <br/> */
            g_string_append_printf(string, "<%s/>", tag_name);
            return TRUE;
        }
        /* start the tag */
        g_string_append_printf(string, "%s<%s", prefix->str, tag_name);
        /* do attributes */
        if (item->attribute_names)
        {
            char **name = item->attribute_names;
            char **value = item->attribute_values;
            while (*name)
            {
                if (*value)
                {
                    char *escaped = g_markup_escape_text(*value, -1);
                    g_string_append_printf(string, " %s='%s'", *name, escaped);
                    g_free(escaped);
                } /* else error? */
                name++;
                value++;
            }
        }
        if (item->children == NULL)
        {
            /* handle empty tags such as <tag attr='value'/> */
            g_string_append(string, "/>");
            *has_nl = TRUE;
            return TRUE;
        }
        g_string_append_c(string, '>');
    }
    /* do with children */
    *has_nl = FALSE; /* to collect data from nested elements */
    g_string_append(prefix, "    ");
    for (l = item->children; l; l = l->next)
        if (!_parser_item_to_gstring(parser, string, l->data, prefix, has_nl, error))
            break;
    g_string_truncate(prefix, prefix->len - 4);
    if (l != NULL) /* failed */
        return FALSE;
    /* close the tag */
    g_string_append_printf(string, "%s</%s>", (*has_nl) ? prefix->str : "",
                           tag_name);
    *has_nl = TRUE; /* it was prefixed above */
    return TRUE;
}

/**
 * fm_xml_parser_to_data
 * @parser: the parser container
 * @text_size: (allow-none) (out): location to save size of returned data
 * @error: (allow-none) (out): location to save error
 *
 * Prepares string representation (XML text) for the data that are in
 * the container. Returned data should be freed with g_free() after
 * usage.
 *
 * Returns: (transfer full): XML text representing data in @parser.
 *
 * Since: 1.2.0
 */
char *fm_xml_parser_to_data(FmXmlParser *parser, gsize *text_size, GError **error)
{
    GString *string, *prefix;
    GList *l;
    gboolean has_nl = FALSE;

    g_return_val_if_fail(parser != NULL && FM_IS_XML_PARSER(parser), NULL);
    string = g_string_sized_new(512);
    prefix = g_string_new("\n");
    if (G_LIKELY(parser->tags[0].name))
        g_string_printf(string, "<!DOCTYPE %s>", parser->tags[0].name);
    for (l = parser->items; l; l = l->next)
        if (!_parser_item_to_gstring(parser, string, l->data, prefix, &has_nl, error))
            break; /* if failed then l != NULL */
    g_string_free(prefix, TRUE);
    if (text_size)
        *text_size = string->len;
    return g_string_free(string, (l != NULL)); /* returns NULL if failed */
}


/* item data accessor functions */

/**
 * fm_xml_parser_item_get_comment
 * @item: the parser element to inspect
 *
 * If an element @item has a comment ahead of it then retrieves that
 * comment. The returned data are owned by @item and should not be freed
 * nor otherwise altered by caller.
 *
 * Returns: (transfer none): comment or %NULL if no comment is set.
 *
 * Since: 1.2.0
 */
const char *fm_xml_parser_item_get_comment(FmXmlParserItem *item)
{
    g_return_val_if_fail(item != NULL, NULL);
    return item->comment;
}

/**
 * fm_xml_parser_get_children
 * @item: the parser element to inspect
 *
 * Retrieves list of children for @item that are known to the parser.
 * Returned list should be freed by g_list_free() after usage.
 *
 * Returns: (transfer container) (element-type FmXmlParserItem): children list.
 *
 * Since: 1.2.0
 */
GList *fm_xml_parser_get_children(FmXmlParserItem *item)
{
    g_return_val_if_fail(item != NULL, NULL);
    return g_list_copy(item->children);
}

/**
 * fm_xml_parser_item_get_tag
 * @item: the parser element to inspect
 *
 * Retrieves tag id of @item.
 *
 * Returns: tag id.
 *
 * Since: 1.2.0
 */
FmXmlParserTag fm_xml_parser_item_get_tag(FmXmlParserItem *item)
{
    g_return_val_if_fail(item != NULL, FM_XML_PARSER_TAG_NOT_HANDLED);
    return item->tag;
}

/**
 * fm_xml_parser_item_get_data
 * @item: the parser element to inspect
 * @text_size: (allow-none) (out): location to save data size
 *
 * Retrieves text data from @item of type FM_XML_PARSER_TEXT. Returned
 * data are owned by parser and should not be freed nor altered.
 *
 * Returns: (transfer none): text data or %NULL if @item isn't text data.
 *
 * Since: 1.2.0
 */
const char *fm_xml_parser_item_get_data(FmXmlParserItem *item, gsize *text_size)
{
    if (text_size)
        *text_size = 0;
    g_return_val_if_fail(item != NULL, NULL);
    if (item->tag != FM_XML_PARSER_TEXT)
        return NULL;
    if (text_size && item->text != NULL)
        *text_size = strlen(item->text);
    return item->text;
}

/**
 * fm_xml_parser_item_get_parent
 * @item: the parser element to inspect
 *
 * Retrieves parent element of @item if the @item has one. Returned data
 * are owned by parser and should not be freed by caller.
 *
 * Returns: (transfer none): parent element or %NULL if element has no parent.
 *
 * Since: 1.2.0
 */
FmXmlParserItem *fm_xml_parser_item_get_parent(FmXmlParserItem *item)
{
    g_return_val_if_fail(item != NULL, NULL);
    return item->parent;
}

/**
 * fm_xml_parser_get_tag_name
 * @parser: the parser container
 * @tag: the tag id to inspect
 *
 * Retrieves tag for its id. Returned data are owned by @parser and should
 * not be modified by caller.
 *
 * Returns: (transfer none): tag string representation.
 *
 * Since: 1.2.0
 */
const char *fm_xml_parser_get_tag_name(FmXmlParser *parser, FmXmlParserTag tag)
{
    g_return_val_if_fail(parser != NULL && FM_IS_XML_PARSER(parser), NULL);
    g_return_val_if_fail(tag > 0 && tag < parser->n_tags, NULL);
    return parser->tags[tag].name;
}
