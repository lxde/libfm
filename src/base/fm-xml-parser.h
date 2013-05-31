/*
 *      fm-xml-parser.h
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

#ifndef __FM_XML_PARSER_H__
#define __FM_XML_PARSER_H__ 1

#define FM_XML_PARSER_TYPE             (_fm_xml_parser_get_type())
#define FM_IS_XML_PARSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), FM_XML_PARSER_TYPE))

G_BEGIN_DECLS

typedef struct _FmXmlParser             FmXmlParser;
typedef struct _FmXmlParserClass        FmXmlParserClass;

GType _fm_xml_parser_get_type(void);

typedef struct _FmXmlParserItem         FmXmlParserItem;
typedef guint                           FmXmlParserTag;

/**
 * FM_XML_PARSER_TAG_NOT_HANDLED:
 *
 * Value of FmXmlParserTag which means this element has no handler installed.
 */
#define FM_XML_PARSER_TAG_NOT_HANDLED 0

/**
 * FM_XML_PARSER_TEXT
 *
 * Value of FmXmlParserTag which means this element has parsed character data.
 */
#define FM_XML_PARSER_TEXT (FmXmlParserTag)-1

/**
 * FmXmlParserHandler
 * @item: XML element being parsed
 * @clildren: (element-type FmXmlParserItem): elements found in @item
 * @attribute_names: attributes names list for @item
 * @attribute_values: attributes values list for @item
 * @n_attributes: list length of @attribute_names and @attribute_values
 * @line: current line number in the file (starting from 1)
 * @pos: current pos number in the file (starting from 0)
 * @error: (allow-none) (out): location to save error
 * @user_data: data passed to fm_xml_parser_parse_data()
 *
 * Callback for processing some element in XML file.
 * It will be called at closing tag.
 *
 * Returns: %TRUE if no errors were found by handler.
 *
 * Since: 1.2.0
 */
typedef gboolean (*FmXmlParserHandler)(FmXmlParserItem *item, GList *clildren,
                                       char * const *attribute_names,
                                       char * const *attribute_values,
                                       guint n_attributes, gint line, gint pos,
                                       GError **error, gpointer user_data);

/* setup */
FmXmlParser *fm_xml_parser_new(FmXmlParser *sibling);
FmXmlParserTag fm_xml_parser_set_handler(FmXmlParser *parser, const char *tag,
                                         FmXmlParserHandler handler,
                                         GError **error);

/* parse */
gboolean fm_xml_parser_parse_data(FmXmlParser *parser, const char *text,
                                  gsize size, GError **error, gpointer user_data);
GList *fm_xml_parser_finish(FmXmlParser *parser, GError **error);
const char *fm_xml_parser_get_dtd(FmXmlParser *parser);

/* item manipulations */
FmXmlParserItem *fm_xml_parser_item_new(FmXmlParserTag tag);
void fm_xml_parser_item_append_text(FmXmlParserItem *item, const char *text,
                                    gssize text_size, gboolean cdata);
gboolean fm_xml_parser_item_append_child(FmXmlParserItem *item, FmXmlParserItem *child);
void fm_xml_parser_item_set_comment(FmXmlParserItem *item, const char *comment);
gboolean fm_xml_parser_item_set_attribute(FmXmlParserItem *item,
                                          const char *name, const char *value);
gboolean fm_xml_parser_item_destroy(FmXmlParserItem *item);

gboolean fm_xml_parser_insert_before(FmXmlParserItem *item, FmXmlParserItem *new_item);
gboolean fm_xml_parser_insert_first(FmXmlParser *parser, FmXmlParserItem *new_item);

/* save XML */
void fm_xml_parser_set_dtd(FmXmlParser *parser, const char *dtd, GError **error);
char *fm_xml_parser_to_data(FmXmlParser *parser, gsize *text_size, GError **error);

/* item data accessor functions */
const char *fm_xml_parser_item_get_comment(FmXmlParserItem *item);
GList *fm_xml_parser_get_children(FmXmlParserItem *item);
FmXmlParserTag fm_xml_parser_item_get_tag(FmXmlParserItem *item);
const char *fm_xml_parser_item_get_data(FmXmlParserItem *item, gsize *text_size);
FmXmlParserItem *fm_xml_parser_item_get_parent(FmXmlParserItem *item);
const char *fm_xml_parser_get_tag_name(FmXmlParser *parser, FmXmlParserTag tag);

G_END_DECLS

#endif /* __FM_XML_PARSER_H__ */
