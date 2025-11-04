#include "list.h"

void print_int(void* data) {
    printf("%d", *((int*)data));
}

void print_string(void* data) {
    printf("%s", (char*)data);
}

int compare_int(void* cur, void* key) {
    return *(int*)cur - *(int*)key;
}

int compare_string(void* cur, void* key) {
    return strcmp((char*)cur, (char*)key);
}

void free_int(void* data) {
}

void free_string(void* data) {
    free(data);
}

void test_int() {
    Node* head = NULL;

    // insert int
    int i1 = 1;
    GenericData idata1 = {&i1, print_int, compare_int, free_int};
    int i2 = 2;
    GenericData idata2 = {&i2, print_int, compare_int, free_int};
    int i3 = 3;
    GenericData idata3 = {&i3, print_int, compare_int, free_int};

    insert(&head, idata1);
    insert(&head, idata2);
    insert(&head, idata3);
    print_list(head);

    int key = 2;
    delete_node(&head, &key);
    print_list(head);

    // clean
    free_list(head);
}

void test_string() {
    Node* head = NULL;
    int len = 8;
    char* s1 = (char*)malloc(sizeof(char)*len);
    strcpy(s1, "hello");
    GenericData sdata1 = {s1, print_string, compare_string, free_string};
    char* s2 = (char*)malloc(sizeof(char)*len);
    strcpy(s2, "world");
    GenericData sdata2 = {s2, print_string, compare_string, free_string};

    insert(&head, sdata1);
    insert(&head, sdata2);
    print_list(head);

    char* strkey = "world";
    delete_node(&head, strkey);
    print_list(head);

    free_list(head);
}





int main() {
    
    test_int();

    test_string();

}