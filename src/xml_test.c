#include <stdio.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "masprintf.h"

/* Wraps an SVG snipped in a dummy document (have to do this for the snippet to
 * parse as an XML doc, unless there happens to be just one element). */

static char * wrap_in_dummy_doc(const char *svg_snippet)
{
	const char * dummy_doc_start = "<dummy>";
	const char * dummy_doc_stop = "</dummy>";
	char *doc_str;


	/* wrap snippet into dummy document */
	size_t start_length = strlen(dummy_doc_start);
	size_t snippet_length = strlen(svg_snippet);
	size_t stop_length = strlen(dummy_doc_stop);
	size_t doc_length = start_length + snippet_length + stop_length;
	/* + 1 for trailing '\0' */
	doc_str = malloc((doc_length + 1) * sizeof(char));
	if (NULL == doc_str) return NULL;

	strcpy(doc_str, dummy_doc_start);
	strcpy(doc_str + start_length, svg_snippet);
	strcpy(doc_str + start_length + snippet_length, dummy_doc_stop);

	return doc_str;
}

/* changes the x-attribute's sign */

static void chs_x_attr(xmlDocPtr doc, char *attr)
{
	/* look for xpath attributes in elements */
	xmlChar *xpath = (xmlChar *) masprintf("//*[@%s]", attr);
	xmlXPathContextPtr context = xmlXPathNewContext(doc);
	xmlXPathObjectPtr result = xmlXPathEvalExpression(xpath, context);
	if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		xmlXPathFreeObject(result);
		xmlXPathFreeContext(context);
		free(xpath);
		return;
	} else {
		xmlNodeSetPtr nodeset = result->nodesetval;
		int i;
		for (i=0; i < nodeset->nodeNr; i++) {
			xmlNodePtr node = nodeset->nodeTab[i];
			xmlChar *value = xmlGetProp(node, (xmlChar *) attr);
			if (NULL != value) {
				double x_val = atof((char *) value);
				x_val *= -1.0;
				char *new_value = masprintf("%g", x_val);
				xmlSetProp(node, (xmlChar *) attr,
						(xmlChar *) new_value);
				free(new_value);
			}	
			xmlFree(value);
		}
		xmlXPathFreeObject (result);
	}
	free(xpath);
	xmlXPathFreeContext(context);
}	

static char *unwrap_snippet(xmlDocPtr doc)
{
	/* this will give a size that is certain to be enough */
	xmlChar *xml_buf;
	int buf_length;
	xmlDocDumpFormatMemory(doc, &xml_buf, &buf_length, 0); 
	xmlFree(xml_buf);	/* only need buf_length */
	/* so, allocate that much (cleared) */
	char *tweaked_svg = calloc(buf_length, sizeof(char));
	if (NULL == tweaked_svg) return NULL;
	// TODO: the following 2 lines can be merged
	xmlNodePtr cur = xmlDocGetRootElement(doc);
	cur = cur->xmlChildrenNode;
	while (NULL != cur) {
		xmlBufferPtr buf = xmlBufferCreate ();
		xmlNodeDump (buf, doc, cur, 0, 0);
		const xmlChar * contents = xmlBufferContent(buf);
		int cur_len = strlen(tweaked_svg);
		/* appends to tweaked_svg - cur_len must initially be 0, which
		 * is why we use calloc() */
		strcpy(tweaked_svg + cur_len, (char *) contents);
		xmlBufferFree(buf);
		cur = cur->next;	/* sibling */
	}
	return tweaked_svg;
}

static char *change_svg_x_attr_sign(const char *svg_snippet)
{
	xmlDocPtr doc;

	char *wrapped_snippet = wrap_in_dummy_doc(svg_snippet);
	if (NULL == wrapped_snippet) { perror(NULL); exit(EXIT_FAILURE); }

	/* parse SVG from string */
	doc = xmlParseMemory(wrapped_snippet, strlen(wrapped_snippet));
	if (NULL == doc) {
		fprintf(stderr, "Failed to parse document\n");
		return NULL;
	}
	free(wrapped_snippet);

	/* See http://www.w3.org/TR/SVG/attindex.html for a list of
	 * x-attributes. */
	chs_x_attr(doc, "cx"); /* circle, ellipse, radialGradient */
	chs_x_attr(doc, "fx"); /* radialGradient */
	// TODO: couldn't we get the same effect by using 'dx' on <text>, etc?
	chs_x_attr(doc, "dx"); /* text, tref, tspan, ... */
	chs_x_attr(doc, "x"); /* text, image ... */
	chs_x_attr(doc, "x1"); /* line, ... */
	chs_x_attr(doc, "x2"); /* line, ... */

	/* now print out the altered snipped, unwrapped.  */
	char *tweaked_svg = unwrap_snippet(doc);
	xmlFreeDoc(doc);

	return tweaked_svg;
}

int main(int argc, char *argv[])
{
	const char *svg_snippet = argv[1];

	char *tweaked_svg = change_svg_x_attr_sign(svg_snippet);
	if (NULL == tweaked_svg) { perror(NULL), exit(EXIT_FAILURE); }
	printf("%s\n", tweaked_svg);
	free(tweaked_svg);

	return EXIT_SUCCESS;
}
