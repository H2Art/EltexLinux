#include <stdlib.h>
#include <string.h>
#include "main.h"

//input
int readLine(FILE *in, char *buf, size_t size) {
    if (!fgets(buf, (int)size, in)) { buf[0] = '\0'; return 0; }
    if (!strchr(buf, '\n')) {                      //string is too long, flush the rest of the line
        int ch;
        while ((ch = getc(in)) != '\n' && ch != EOF) { }
    }
    buf[strcspn(buf, "\n")] = '\0';
    return 1;
}

int readInt(FILE *in, int *out) {
    char buf[16];
    if (!readLine(in, buf, sizeof buf)) return 0;
    *out = (int)strtol(buf, NULL, 10);             //non-numeric input will be converted to 0
    return 1;
}



//output
void printMainOptions(void) {
    printf("\nChoose an option:\n"
           "1. Add contact\n"
           "2. List contacts\n"
           "3. Edit current contact\n"
           "4. Delete current contact\n"
           "5. Print current contact\n"
           "6. Select contact by index\n"
           "7. Show tree structure\n"
           "9. Exit\n");
}

void printEditOptions(void) {
    printf("What do you want to edit?\n"
           "1. Phones\n2. Emails\n3. Links\n4. Name\n5. Work information\n6. Back\n");
}

void printContact(const Contact *c) {
    printf("Name: %s %s %s\n", c->name.lastName, c->name.firstName, c->name.middleName);
    printf("Work: %s at %s\n", c->workInfo.position, c->workInfo.company);
    printf("Phones (%d):\n", c->phoneCount);
    for (int i = 0; i < c->phoneCount; i++) printf("  %s\n", c->phones[i]);
    printf("Emails (%d):\n", c->emailCount);
    for (int i = 0; i < c->emailCount; i++) printf("  %s\n", c->emails[i]);
    printf("Links (%d):\n", c->linkCount);
    for (int i = 0; i < c->linkCount; i++) printf("  %s\n", c->links[i]);
}

void printList(const ContactTree *t) {
    if (t->count == 0) { printf("Contact list is empty.\n"); return; }
    int i = 0;
    for (const Node *p = treeMin(t->root); p; p = treeNext(p), i++) {
        printf("%d. %s %s %s\n", i, p->data.name.lastName, p->data.name.firstName,
               p->data.name.middleName);
    }
}

// Sideways tree: right subtree on top, left on the bottom, root at the left edge
static void printTreeRec(const Node *n, int depth) {
    if (!n) return;
    printTreeRec(n->right, depth + 1);
    printf("%*s[%c] %s %s %s\n", depth * 6, "", n->color == RB_RED ? 'R' : 'B',
           n->data.name.lastName, n->data.name.firstName, n->data.name.middleName);
    printTreeRec(n->left, depth + 1);
}

void printTree(const ContactTree *t) {
    if (t->count == 0) { printf("Tree is empty.\n"); return; }
    printf("Tree (root at the left, right subtree on top; R = red, B = black):\n");
    printTreeRec(t->root, 0);
    int bh = treeValidate(t);
    if (bh < 0) printf("Validation: FAILED\n");
    else        printf("Validation: OK (nodes: %d, black height: %d)\n", t->count, bh);
}

//dialog functions
int askName(Contact *c, FILE *in) {
    char f[MAX_LENGTH], m[MAX_LENGTH], l[MAX_LENGTH];
    printf("First name (required): ");  if (!readLine(in, f, sizeof f)) return CONTACT_ERR_NULL_ARG;
    printf("Middle name (optional): "); if (!readLine(in, m, sizeof m)) return CONTACT_ERR_NULL_ARG;
    printf("Last name (required): ");   if (!readLine(in, l, sizeof l)) return CONTACT_ERR_NULL_ARG;
    return contactSetName(c, f, m, l);
}

int askWork(Contact *c, FILE *in) {
    char p[MAX_LENGTH], co[MAX_LENGTH];
    printf("Position: "); if (!readLine(in, p, sizeof p))   return CONTACT_ERR_NULL_ARG;
    printf("Company: ");  if (!readLine(in, co, sizeof co)) return CONTACT_ERR_NULL_ARG;
    return contactSetWork(c, p, co);
}

int askItems(Contact *c, ItemType type, FILE *in) {
    int n;
    printf("How many entries (1-%d)? Old ones will be replaced.\n", MAX_ITEMS);
    if (!readInt(in, &n)) return CONTACT_ERR_NULL_ARG;
    if (n < 1 || n > MAX_ITEMS) return CONTACT_ERR_BAD_COUNT;

    char buffer[MAX_ITEMS][MAX_LENGTH];
    printf("Type %d items, ENTER after each:\n", n);
    for (int i = 0; i < n; i++) {
        if (!readLine(in, buffer[i], MAX_LENGTH)) return CONTACT_ERR_EMPTY_ITEM;
    }
    return contactSetItems(c, type, n, buffer);
}


//
static void report(int status) {
    if (status != CONTACT_OK) printf("Error: %s\n", contactStatusString(status));
}

int processEditChoice(AppState *st, int choice, FILE *in) {
    if (!st->current) { printf("No contact selected.\n"); return CONTACT_ERR_NULL_ARG; }

    Contact tmp = st->current->data;     //modify a copy, and only if everything is ok, save it back to the list
    int result;

    switch (choice) {
    case EO_PHONES: result = askItems(&tmp, ITEM_PHONE, in); break;
    case EO_EMAILS: result = askItems(&tmp, ITEM_EMAIL, in); break;
    case EO_LINKS:  result = askItems(&tmp, ITEM_LINK,  in); break;
    case EO_NAME:   result = askName(&tmp, in);              break;
    case EO_WORK:   result = askWork(&tmp, in);              break;
    case EO_BACK:   return CONTACT_OK;
    default:        printf("Unknown option.\n"); return CONTACT_ERR_UNKNOWN_TYPE;
    }

    if (result == CONTACT_OK) {
        result = treeUpdate(&st->tree, st->current, &tmp);   //duplicate check, reposition and rebalance
        if (result == CONTACT_OK) printf("Saved.\n");
    }
    return result;
}

int processMainChoice(AppState *st, int choice, FILE *in) {
    int result = CONTACT_OK;

    switch (choice) {
    case UO_ADD: {
        Contact c = {0};
        result = askName(&c, in);
        if (result == CONTACT_OK) result = askWork(&c, in);
        if (result == CONTACT_OK) {
            Node *n = NULL;
            result = treeInsert(&st->tree, &c, &n);
            if (result == CONTACT_OK) {
                st->current = n;
                printf("Contact added and selected. Use 'Edit' to add phones, emails, links.\n");
            }
        }
        break;
    }
    case UO_LIST:
        printList(&st->tree);
        break;
    case UO_EDIT:
        printEditOptions();
        {
            int sub;
            if (!readInt(in, &sub)) { st->running = 0; return CONTACT_OK; }
            result = processEditChoice(st, sub, in);
        }
        break;
    case UO_DELETE:
        if (!st->current) { printf("No contact selected.\n"); return CONTACT_ERR_NULL_ARG; }
        result = treeRemove(&st->tree, st->current);
        if (result == CONTACT_OK) { st->current = NULL; printf("Deleted.\n"); }
        break;
    case UO_PRINT:
        if (!st->current) { printf("No contact selected.\n"); return CONTACT_ERR_NULL_ARG; }
        printContact(&st->current->data);
        break;
    case UO_SELECT: {
        if (st->tree.count == 0) { printf("Contact list is empty.\n"); return CONTACT_ERR_BAD_INDEX; }
        printList(&st->tree);
        printf("Index (0-%d): ", st->tree.count - 1);
        int idx;
        if (!readInt(in, &idx)) { st->running = 0; return CONTACT_OK; }
        Node *n = treeGetAt(&st->tree, idx);
        if (!n) { result = CONTACT_ERR_BAD_INDEX; break; }
        st->current = n;
        printf("Contact %d selected.\n", idx);
        break;
    }
    case UO_TREE:
        printTree(&st->tree);
        break;
    case UO_EXIT:
        st->running = 0;
        break;
    default:
        printf("Unknown option.\n");
        return CONTACT_ERR_UNKNOWN_TYPE;
    }

    report(result);
    return result;
}

int main(void) {
    AppState st = {0};
    st.running = 1;
    while (st.running) {
        int choice;
        printMainOptions();
        if (!readInt(stdin, &choice)) break;      //EOF
        processMainChoice(&st, choice, stdin);
    }
    treeFree(&st.tree);
    return 0;
}