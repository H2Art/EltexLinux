#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "Contacts.h"

const char *contactStatusString(int s) {
    switch (s) {
    case CONTACT_OK:                   return "OK";
    case CONTACT_ERR_NULL_ARG:         return "Null argument";
    case CONTACT_ERR_EMPTY_FIRST_NAME: return "First name is required";
    case CONTACT_ERR_EMPTY_LAST_NAME:  return "Last name is required";
    case CONTACT_ERR_BAD_INDEX:        return "Index out of range";
    case CONTACT_ERR_BAD_COUNT:        return "Wrong number of items";
    case CONTACT_ERR_UNKNOWN_TYPE:     return "Unknown item type";
    case CONTACT_ERR_EMPTY_ITEM:       return "Empty item";
    case CONTACT_ERR_NO_MEMORY:        return "Out of memory";
    case CONTACT_ERR_DUPLICATE:        return "Contact with this name already exists";
    default:                           return "Unknown error";
    }
}

int isBlank(const char *s) {
    if (!s) return 1;
    for (; *s; s++) {
        if (!isspace((unsigned char)*s)) return 0;
    }
    return 1;
}

int contactSetName(Contact *c, const char *first, const char *middle, const char *last) {
    if (!c) return CONTACT_ERR_NULL_ARG;
    if (isBlank(first)) return CONTACT_ERR_EMPTY_FIRST_NAME;
    if (isBlank(last))  return CONTACT_ERR_EMPTY_LAST_NAME;

    Fullname fn;
    snprintf(fn.firstName,  sizeof fn.firstName,  "%s", first);
    snprintf(fn.middleName, sizeof fn.middleName, "%s", middle ? middle : "");
    snprintf(fn.lastName,   sizeof fn.lastName,   "%s", last);
    c->name = fn;
    return CONTACT_OK;
}

int contactSetWork(Contact *c, const char *position, const char *company) {
    if (!c) return CONTACT_ERR_NULL_ARG;
    snprintf(c->workInfo.position, sizeof c->workInfo.position, "%s", position ? position : "");
    snprintf(c->workInfo.company,  sizeof c->workInfo.company,  "%s", company  ? company  : "");
    return CONTACT_OK;
}

int contactSetItems(Contact *c, ItemType type, int n, const char items[][MAX_LENGTH]) {
    if (!c || !items) return CONTACT_ERR_NULL_ARG;

    char (*dst)[MAX_LENGTH];
    int *counter;
    switch (type) {
    case ITEM_PHONE: dst = c->phones; counter = &c->phoneCount; break;
    case ITEM_EMAIL: dst = c->emails; counter = &c->emailCount; break;
    case ITEM_LINK:  dst = c->links;  counter = &c->linkCount;  break;
    default: return CONTACT_ERR_UNKNOWN_TYPE;
    }

    if (n < 1 || n > MAX_ITEMS) return CONTACT_ERR_BAD_COUNT;
    for (int i = 0; i < n; i++) {
        if (isBlank(items[i])) return CONTACT_ERR_EMPTY_ITEM;
    }

    memset(dst, 0, (size_t)MAX_ITEMS * MAX_LENGTH);
    for (int i = 0; i < n; i++) {
        snprintf(dst[i], MAX_LENGTH, "%s", items[i]);
    }
    *counter = n;
    return CONTACT_OK;
}

// Key: Lastname; if equal, then Firstname, then Middlename
int contactCompare(const Contact *a, const Contact *b) {
    int r = strcasecmp(a->name.lastName, b->name.lastName);
    if (r) return r;
    r = strcasecmp(a->name.firstName, b->name.firstName);
    if (r) return r;
    return strcasecmp(a->name.middleName, b->name.middleName);
}



//Tree functions (red-black tree, nodes have parent pointers, NULL is a black leaf)
static void rotateLeft(ContactTree *t, Node *x) {
    Node *y = x->right;
    x->right = y->left;
    if (y->left) y->left->parent = x;
    y->parent = x->parent;
    if (!x->parent) t->root = y;
    else if (x == x->parent->left) x->parent->left = y;
    else x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void rotateRight(ContactTree *t, Node *x) {
    Node *y = x->left;
    x->left = y->right;
    if (y->right) y->right->parent = x;
    y->parent = x->parent;
    if (!x->parent) t->root = y;
    else if (x == x->parent->right) x->parent->right = y;
    else x->parent->left = y;
    y->right = x;
    x->parent = y;
}

static int isRed(const Node *n) { return n && n->color == RB_RED; }

static void insertFixup(ContactTree *t, Node *z) {
    while (z->parent && z->parent->color == RB_RED) {
        Node *p = z->parent;
        Node *g = p->parent;                 // red parent is never the root, so g exists
        if (p == g->left) {
            Node *u = g->right;
            if (isRed(u)) {
                p->color = RB_BLACK; u->color = RB_BLACK; g->color = RB_RED;
                z = g;
            } else {
                if (z == p->right) { z = p; rotateLeft(t, z); p = z->parent; }
                p->color = RB_BLACK; g->color = RB_RED;
                rotateRight(t, g);
            }
        } else {
            Node *u = g->left;
            if (isRed(u)) {
                p->color = RB_BLACK; u->color = RB_BLACK; g->color = RB_RED;
                z = g;
            } else {
                if (z == p->left) { z = p; rotateRight(t, z); p = z->parent; }
                p->color = RB_BLACK; g->color = RB_RED;
                rotateLeft(t, g);
            }
        }
    }
    t->root->color = RB_BLACK;
}

// Link an already allocated node into the tree and rebalance. Key must not exist yet
static void treeAttach(ContactTree *t, Node *n) {
    Node *p = NULL, *c = t->root;
    int cmp = 0;
    while (c) {
        p = c;
        cmp = contactCompare(&n->data, &c->data);
        c = cmp < 0 ? c->left : c->right;
    }
    n->parent = p;
    n->left = n->right = NULL;
    n->color = RB_RED;
    if (!p) t->root = n;
    else if (cmp < 0) p->left = n;
    else p->right = n;
    insertFixup(t, n);
    t->count++;
}

static void transplant(ContactTree *t, Node *u, Node *v) {
    if (!u->parent) t->root = v;
    else if (u == u->parent->left) u->parent->left = v;
    else u->parent->right = v;
    if (v) v->parent = u->parent;
}

// x may be NULL (black leaf), so its parent is passed explicitly
static void deleteFixup(ContactTree *t, Node *x, Node *xp) {
    while (x != t->root && !isRed(x)) {
        if (x == xp->left) {
            Node *w = xp->right;
            if (isRed(w)) {
                w->color = RB_BLACK; xp->color = RB_RED;
                rotateLeft(t, xp);
                w = xp->right;
            }
            if (!isRed(w->left) && !isRed(w->right)) {
                w->color = RB_RED;
                x = xp; xp = x->parent;
            } else {
                if (!isRed(w->right)) {
                    w->left->color = RB_BLACK; w->color = RB_RED;
                    rotateRight(t, w);
                    w = xp->right;
                }
                w->color = xp->color; xp->color = RB_BLACK;
                if (w->right) w->right->color = RB_BLACK;
                rotateLeft(t, xp);
                x = t->root;
            }
        } else {
            Node *w = xp->left;
            if (isRed(w)) {
                w->color = RB_BLACK; xp->color = RB_RED;
                rotateRight(t, xp);
                w = xp->left;
            }
            if (!isRed(w->left) && !isRed(w->right)) {
                w->color = RB_RED;
                x = xp; xp = x->parent;
            } else {
                if (!isRed(w->left)) {
                    w->right->color = RB_BLACK; w->color = RB_RED;
                    rotateLeft(t, w);
                    w = xp->left;
                }
                w->color = xp->color; xp->color = RB_BLACK;
                if (w->left) w->left->color = RB_BLACK;
                rotateRight(t, xp);
                x = t->root;
            }
        }
    }
    if (x) x->color = RB_BLACK;
}

// Unlink node from the tree and rebalance. The node itself is NOT freed and is not copied/moved,
// so pointers to other nodes stay valid
static void treeDetach(ContactTree *t, Node *z) {
    Node *y = z, *x, *xp;
    NodeColor yColor = y->color;

    if (!z->left) {
        x = z->right; xp = z->parent;
        transplant(t, z, z->right);
    } else if (!z->right) {
        x = z->left; xp = z->parent;
        transplant(t, z, z->left);
    } else {
        y = (Node *)treeMin(z->right);
        yColor = y->color;
        x = y->right;
        if (y->parent == z) {
            xp = y;
        } else {
            xp = y->parent;
            transplant(t, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        transplant(t, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }
    if (yColor == RB_BLACK) deleteFixup(t, x, xp);

    z->left = z->right = z->parent = NULL;
    t->count--;
}

Node *treeFind(const ContactTree *t, const Contact *key) {
    Node *p = t ? t->root : NULL;
    while (p) {
        int cmp = contactCompare(key, &p->data);
        if (cmp == 0) return p;
        p = cmp < 0 ? p->left : p->right;
    }
    return NULL;
}

int treeInsert(ContactTree *t, const Contact *c, Node **out) {
    if (!t || !c) return CONTACT_ERR_NULL_ARG;
    if (isBlank(c->name.firstName)) return CONTACT_ERR_EMPTY_FIRST_NAME;
    if (isBlank(c->name.lastName))  return CONTACT_ERR_EMPTY_LAST_NAME;
    if (treeFind(t, c)) return CONTACT_ERR_DUPLICATE;

    Node *n = malloc(sizeof *n);
    if (!n) return CONTACT_ERR_NO_MEMORY;
    n->data = *c;
    treeAttach(t, n);
    if (out) *out = n;
    return CONTACT_OK;
}

int treeUpdate(ContactTree *t, Node *n, const Contact *newData) {
    if (!t || !n || !newData) return CONTACT_ERR_NULL_ARG;
    if (isBlank(newData->name.firstName)) return CONTACT_ERR_EMPTY_FIRST_NAME;
    if (isBlank(newData->name.lastName))  return CONTACT_ERR_EMPTY_LAST_NAME;

    Node *dup = treeFind(t, newData);
    if (dup && dup != n) return CONTACT_ERR_DUPLICATE;      // nothing has been changed yet

    if (contactCompare(&n->data, newData) == 0) {           // position in the tree is the same
        n->data = *newData;
        return CONTACT_OK;
    }
    treeDetach(t, n);                                       // key changed: move the node
    n->data = *newData;
    treeAttach(t, n);                                       // rebalancing happens here
    return CONTACT_OK;
}

int treeRemove(ContactTree *t, Node *n) {
    if (!t || !n) return CONTACT_ERR_NULL_ARG;
    treeDetach(t, n);
    free(n);
    return CONTACT_OK;
}

Node *treeMin(const Node *n) {
    if (!n) return NULL;
    while (n->left) n = n->left;
    return (Node *)n;
}

Node *treeNext(const Node *n) {
    if (!n) return NULL;
    if (n->right) return treeMin(n->right);
    const Node *p = n->parent;
    while (p && n == p->right) { n = p; p = p->parent; }
    return (Node *)p;
}

Node *treeGetAt(const ContactTree *t, int index) {
    if (!t || index < 0 || index >= t->count) return NULL;
    Node *p = treeMin(t->root);
    for (int i = 0; i < index; i++) p = treeNext(p);
    return p;
}

static void freeSubtree(Node *n) {
    if (!n) return;
    freeSubtree(n->left);
    freeSubtree(n->right);
    free(n);
}

void treeFree(ContactTree *t) {
    freeSubtree(t->root);
    t->root = NULL;
    t->count = 0;
}

// Returns black height of the subtree or -1
static int checkSubtree(const Node *n) {
    if (!n) return 1;
    if (n->left && n->left->parent != n) return -1;
    if (n->right && n->right->parent != n) return -1;
    if (isRed(n) && (isRed(n->left) || isRed(n->right))) return -1;
    int l = checkSubtree(n->left);
    int r = checkSubtree(n->right);
    if (l < 0 || r < 0 || l != r) return -1;
    return l + (n->color == RB_BLACK ? 1 : 0);
}

int treeValidate(const ContactTree *t) {
    if (!t) return -1;
    if (t->root) {
        if (t->root->parent || isRed(t->root)) return -1;
    }
    int bh = checkSubtree(t->root);
    if (bh < 0) return -1;

    int n = 0;
    const Node *prev = NULL;
    for (const Node *p = treeMin(t->root); p; p = treeNext(p)) {
        if (prev && contactCompare(&prev->data, &p->data) >= 0) return -1;   // strictly increasing
        prev = p;
        n++;
    }
    return n == t->count ? bh : -1;
}