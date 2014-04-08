#include "util/cjson.h"

#define BVT_GLIFFY_SPLIT "|"

static const char* const BVT_GLIFFY_TYPE_STAGE = "stage";
static const char* const BVT_GLIFFY_TYPE_OBJECTS = "objects";
static const char* const BVT_GLIFFY_TYPE_ID = "id";
static const char* const BVT_GLIFFY_TYPE_GRAPHIC = "graphic";
static const char* const BVT_GLIFFY_TYPE_TYPE = "type";
static const char* const BVT_GLIFFY_TYPE_CHILDREN = "children";
static const char* const BVT_GLIFFY_TYPE_HTML = "html";
static const char* const BVT_GLIFFY_TYPE_TEXT = "Text";
static const char* const BVT_GLIFFY_TYPE_CONSTRAINTS = "constraints";
static const char* const BVT_GLIFFY_TYPE_CONSTRAINTS_START = "startConstraint";
static const char* const BVT_GLIFFY_TYPE_CONSTRAINTS_END = "endConstraint";
static const char* const BVT_GLIFFY_TYPE_CONSTRAINTS_START_POS = "StartPositionConstraint";
static const char* const BVT_GLIFFY_TYPE_CONSTRAINTS_END_POS = "EndPositionConstraint";
static const char* const BVT_GLIFFY_TYPE_NODEID = "nodeId";

static const char* const BVT_GLIFFY_VALUE_SHAPE = "Shape";
static const char* const BVT_GLIFFY_VALUE_LINE = "Line";
static const char* const BVT_GLIFFY_VALUE_TEXT = "Text";

enum {
    BVT_GRAPH_NODE_SHAPE,
    BVT_GRAPH_NODE_LINE,
};

typedef struct bvt_graph_node_t {
    int32_t t;
    int32_t id;
    union {
        struct {
            char desc[BVT_MAX_NAME_LEN];
        } shape;
        struct {
            int32_t from;
            int32_t to;
        } line;
    };
    struct bvt_graph_node_t* next;
} bvt_graph_node_t;

typedef struct bvt_graph_t {
    struct bvt_graph_node_t* head;
} bvt_graph_t;

// trim html tags
static void
_bvt_load_gliffy_html(const char* html, char* dst, size_t dstlen)
{
    int32_t flag = 0;
    int32_t trans = 0;
    size_t dlen = 0;
    assert(html);
    while (*html) {
        if (dlen >= dstlen - 1) break;
        if (trans == 1) { trans = 0; ++ html; continue; }
        if (*html == '<') { ++ flag; ++ html; continue; }
        if (*html == '\\') { trans = 1; ++ html; continue; }
        if (*html == '>' && flag > 0) { -- flag; ++ html; continue; }
        if (flag == 0) { dst[dlen++] = *html; }
        ++html;
    }
    dst[dlen] = 0;
}

#undef BVT_JSON_GO_DOWN
#define BVT_JSON_GO_DOWN(js, jt, name) \
    do { \
        (js) = (js)->child; \
        while (js) { \
            if ((js)->type == (jt) && 0 == strcmp((js)->string, (name))) \
                break; \
            (js) = (js)->next; \
        } \
    } while (0)

static bvt_graph_node_t*
_bvt_load_gliffy_node(cJSON* js) {
    cJSON* js_id, *js_graphic, *js_type, *js_children, *c;
    cJSON* js_child_graphic, *js_child_type, *js_child_text, *js_child_text_html;
    bvt_graph_node_t* node = NULL;
    cJSON* js_cst, *js_cst_start, *js_cst_start_pos, *js_cst_start_id;
    cJSON* js_cst_end, *js_cst_end_pos, *js_cst_end_id;

    js_id = js;
    BVT_JSON_GO_DOWN(js_id, cJSON_Number, BVT_GLIFFY_TYPE_ID);
    if (!js_id) return NULL;

    js_graphic = js;
    BVT_JSON_GO_DOWN(js_graphic, cJSON_Object, BVT_GLIFFY_TYPE_GRAPHIC);
    if (!js_graphic) return NULL;

    js_type = js_graphic;
    BVT_JSON_GO_DOWN(js_type, cJSON_String, BVT_GLIFFY_TYPE_TYPE);
    if (!js_type) return NULL;

    node = (bvt_graph_node_t*)MALLOC(sizeof(bvt_graph_node_t));
    node->id = js_id->valueint;
    node->next = NULL;

    // load shape desc
    if (0 == strcmp(js_type->valuestring, BVT_GLIFFY_VALUE_SHAPE)) {
        node->t = BVT_GRAPH_NODE_SHAPE;

        // children list
        js_children = js;
        BVT_JSON_GO_DOWN(js_children, cJSON_Array, BVT_GLIFFY_TYPE_CHILDREN);
        if (!js_children) goto GLIFFY_FAIL;

        // child object
        c = js_children->child;
        if (!c) goto GLIFFY_FAIL;

        while (c) {
            // child graphic
            js_child_graphic = c;
            BVT_JSON_GO_DOWN(js_child_graphic, cJSON_Object, BVT_GLIFFY_TYPE_GRAPHIC);
            if (!js_child_graphic) goto GLIFFY_FAIL;

            // child graphic type
            js_child_type = js_child_graphic;
            BVT_JSON_GO_DOWN(js_child_type, cJSON_String, BVT_GLIFFY_TYPE_TYPE);
            if (!js_child_type) goto GLIFFY_FAIL;
            if (strcmp(js_child_type->valuestring, BVT_GLIFFY_VALUE_TEXT)) {
                c = c->next;
                continue;
            }

            // child graphic text
            js_child_text = js_child_graphic;
            BVT_JSON_GO_DOWN(js_child_text, cJSON_Object, BVT_GLIFFY_TYPE_TEXT);
            if (!js_child_text) goto GLIFFY_FAIL;

            // child graphic text html
            js_child_text_html = js_child_text;
            BVT_JSON_GO_DOWN(js_child_text_html, cJSON_String, BVT_GLIFFY_TYPE_HTML);
            if (!js_child_text_html) goto GLIFFY_FAIL;

            // set shape value
            _bvt_load_gliffy_html(js_child_text_html->valuestring, node->shape.desc,
                sizeof(node->shape.desc));
            break;
        }
        if (!c) goto GLIFFY_FAIL;
    }

    // load line end points
    else if (0 == strcmp(js_type->valuestring, BVT_GLIFFY_VALUE_LINE)) {

        node->t = BVT_GRAPH_NODE_LINE;

        // child constraints
        js_cst = js;
        BVT_JSON_GO_DOWN(js_cst, cJSON_Object, BVT_GLIFFY_TYPE_CONSTRAINTS);
        if (!js_cst) goto GLIFFY_FAIL;

        // child constraints start
        js_cst_start = js_cst;
        BVT_JSON_GO_DOWN(js_cst_start, cJSON_Object, BVT_GLIFFY_TYPE_CONSTRAINTS_START);
        if (!js_cst_start) goto GLIFFY_FAIL;
        js_cst_start_pos = js_cst_start;
        BVT_JSON_GO_DOWN(js_cst_start_pos, cJSON_Object, BVT_GLIFFY_TYPE_CONSTRAINTS_START_POS);
        if (!js_cst_start_pos) goto GLIFFY_FAIL;
        js_cst_start_id = js_cst_start_pos;
        BVT_JSON_GO_DOWN(js_cst_start_id, cJSON_Number, BVT_GLIFFY_TYPE_NODEID);
        if (!js_cst_start_id) goto GLIFFY_FAIL;
        node->line.from = js_cst_start_id->valueint;

        // child constaints end
        js_cst_end = js_cst;
        BVT_JSON_GO_DOWN(js_cst_end, cJSON_Object, BVT_GLIFFY_TYPE_CONSTRAINTS_END);
        if (!js_cst_end) goto GLIFFY_FAIL;
        js_cst_end_pos = js_cst_end;
        BVT_JSON_GO_DOWN(js_cst_end_pos, cJSON_Object, BVT_GLIFFY_TYPE_CONSTRAINTS_END_POS);
        if (!js_cst_end_pos) goto GLIFFY_FAIL;
        js_cst_end_id = js_cst_end_pos;
        BVT_JSON_GO_DOWN(js_cst_end_id, cJSON_Number, BVT_GLIFFY_TYPE_NODEID);
        if (!js_cst_end_id) goto GLIFFY_FAIL;
        node->line.to = js_cst_end_id->valueint;
    }

    // unrecognized gliffy type
    else {
        goto GLIFFY_FAIL;
    }

    return node;

GLIFFY_FAIL:
    FREE(node);
    return NULL;
}

static int32_t
_bvt_load_gliffy_parse_name(bvt_t* node, char* name) {
    const char* split = BVT_GLIFFY_SPLIT;
    char* p = 0;
    if (!node || !name) return BVT_ERROR;

    // type
    p = strtok(name, split);
    if (!p) return BVT_ERROR;
    if (0 == strcmp(p, "SEQ")) {
        node->type = BVT_NODE_SEQUENCE;
    } else if (0 == strcmp(p, "SEL")) {
        node->type = BVT_NODE_SELECTOR;
        node->sel_args.type = BVT_SELECTOR_COND;
    } else if (0 == strcmp(p, "SEL_WEIGHT")) {
        node->type = BVT_NODE_SELECTOR;
        node->sel_args.type = BVT_SELECTOR_WEIGHT;
    } else if (0 == strcmp(p, "SEL_COND")) {
        node->type = BVT_NODE_SELECTOR;
        node->sel_args.type = BVT_SELECTOR_COND;
    } else if (0 == strcmp(p, "PAR")) {
        node->type = BVT_NODE_PARALLEL;
    } else if (0 == strcmp(p, "ACT")) {
        node->type = BVT_NODE_ACTION;
    } else if (0 == strcmp(p, "COND")) {
        node->type = BVT_NODE_CONDITION;
    } else if (0 == strcmp(p, "PAR_ALL")) {
        node->type = BVT_NODE_PARALLEL;
        node->par_args.type = BVT_PARALLEL_ALL;
    } else if (0 == strcmp(p, "PAR_ONE")) {
        node->type = BVT_NODE_PARALLEL;
        node->par_args.type = BVT_PARALLEL_ONE;
    } else {
        return BVT_ERROR;
    }

    // desc
    p = strtok(NULL, split);
    snprintf(node->name, sizeof(node->name), "%s", p);

    // callback id
    p = strtok(NULL, split);
    if (p) {
        if (p[0] == 'W' || p[0] == 'w')
            goto BVT_PARSE_WEIGHT;
        if (node->type == BVT_NODE_ACTION)
            node->act_args.callback_id = atoi(p);
        else if (node->type == BVT_NODE_CONDITION)
            node->con_args.callback_id = atoi(p);
        else
            return BVT_ERROR;
    }

    // weight;
    p = strtok(NULL, split);
    if (!p) return BVT_SUCCESS;
    if (p[0] != 'W' && p[0] != 'w') return BVT_ERROR;

BVT_PARSE_WEIGHT:
    node->weight = atoi(p + 1);
    return BVT_SUCCESS;
}

static struct bvt_t*
_bvt_load_gliffy_graph_node(int32_t id, bvt_graph_node_t** list) {
    bvt_graph_node_t* node, *prev;
    bvt_t* b = NULL;
    bvt_graph_node_t* line = NULL;
    bvt_t* child = NULL;
    int32_t ret;
    if (!list || !*list) return NULL;

    // find and split a node
    node = *list;
    prev = 0;
    while (node) {
        if (node->t == BVT_GRAPH_NODE_SHAPE && node->id == id) {
            if (prev) prev->next = node->next;
            else *list = (*list)->next;
            break;
        }
        prev = node;
        node = node->next;
    }
    if (!node) return NULL;

    // set node data
    b = (bvt_t*)MALLOC(sizeof(bvt_t));
    memset(b, 0, sizeof(bvt_t));
    ret = _bvt_load_gliffy_parse_name(b, node->shape.desc);
    if (ret != BVT_SUCCESS) {
        printf("%s error\n", node->shape.desc);
        assert(0);
    }
    // release graph nodes
    FREE(node);

    // find its children
    while (1) {
        line = *list;
        prev = 0;
        while (line) {
            if (line->t == BVT_GRAPH_NODE_LINE && line->line.from == id) {
                if (prev) prev->next = line->next;
                else *list = (*list)->next;
                break;
            }
            prev = line;
            line = line->next;
        }
        if (!line) break;

        child = _bvt_load_gliffy_graph_node(line->line.to, list);
        assert(child);
        if (child->type == BVT_NODE_CONDITION) {
            if (b->condition) {
                child->next = b->condition;
            }
            b->condition = child;
        } else {
            if (b->child) {
                child->next = b->child;
            }
            b->child = child;
        }

        // release graph nodes
        FREE(line);
    }
    return b;
}

#if 0
static void
_bvt_debug_graph(bvt_graph_t* g) {
    // printf debug
    bvt_graph_node_t* n = g->head;
    while (n) {
        if (n->t == BVT_GRAPH_NODE_SHAPE) {
            printf("shape[%d]: %s\n", n->id, n->shape.desc);
        } else if (n->t == BVT_GRAPH_NODE_LINE) {
            printf("line[%d->%d]\n", n->line.from, n->line.to);
        }
        n = n->next;
    }
}
#endif

// init bvt by graph
static bvt_t*
_bvt_load_gliffy_graph(bvt_graph_t* g) {
    // get root id
    bvt_graph_node_t* n = g->head;
    bvt_graph_node_t* find = NULL;
    bvt_graph_node_t* b = NULL;
    while (n) {
        if (n->t == BVT_GRAPH_NODE_LINE) {
            n = n->next;
            continue;
        }
        b = g->head;
        while (b) {
            if (b->t == BVT_GRAPH_NODE_LINE && b->line.to == n->id) {
                break;
            }
            b = b->next;
        }
        if (!b && find) {
            printf("duplicate root found: [%s], [%s] \n", n->shape.desc, find->shape.desc);
            return NULL;
        }
        if (!b) { find = n; }
        n = n->next;
    }
    if (!find) {
        printf("no root node found!\n");
        return NULL;
    }
    return _bvt_load_gliffy_graph_node(find->id, &g->head);
}

// gliffy is also json format, but with lots of view info
// we need to get what we need
static bvt_t*
_bvt_load_gliffy(cJSON* js) {
    bvt_graph_node_t* node, *head;
    cJSON* c = NULL;
    bvt_graph_t g;
    memset(&g, 0, sizeof(g));

    if (js->type != cJSON_Object)
        return NULL;

    // stage node
    BVT_JSON_GO_DOWN(js, cJSON_Object, BVT_GLIFFY_TYPE_STAGE);
    if (!js) return NULL;

    // objects node
    BVT_JSON_GO_DOWN(js, cJSON_Array, BVT_GLIFFY_TYPE_OBJECTS);
    if (!js) return NULL;

    head = g.head;
    c = js->child;
    while (c) {
        if (c->type == cJSON_Object) {
            node = _bvt_load_gliffy_node(c);
            if (node) {
                if (head) {
                    head->next = node;
                    head = node;
                } else {
                    g.head = node;
                    head = node;
                }
            }
        }
        c = c->next;
    }

    // init bvt by graph
    return _bvt_load_gliffy_graph(&g);
}

// init by gliffy file (also json format, but with lots of vision info)
struct bvt_t*
bvt_load_gliffy(const char* cfg) {
    int fd;
    off_t size;
    char* src = NULL;
    cJSON* js = NULL;
    bvt_t* root = NULL;

    if (!cfg) return NULL;
    fd = open(cfg, O_RDONLY);
    if (fd < 0) return NULL;
    size = lseek(fd, 0, SEEK_END);
    src = (char*)MALLOC(size + 1);
    lseek(fd, 0, SEEK_SET);
    read(fd, src, size);
    src[size] = 0;

    // read gliffy config
    js = cJSON_Parse(src);
    if (!js) {
        printf("gliffy config %s error\n", cfg);
        return NULL;
    }

    // load gliffy relation to bvt and check
    root = _bvt_load_gliffy(js);
    cJSON_Delete(js);
    FREE(src);
    return root;
}

