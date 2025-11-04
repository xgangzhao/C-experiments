#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GenericData{
    void* data;
    void (*print_data) (void*);
    int (*compare_data) (void*, void*);
    void (*free_data) (void*);
} GenericData;

typedef struct Node {
    GenericData data;
    struct Node* next;
} Node;

Node* create_node(GenericData data) {
    Node* node = (Node*)malloc(sizeof(Node));
    if (!node) {
        printf("memory allocation failed!\n");
        exit(1);
    }
    node->data = data;
    node->next = NULL;
    return node;
}

void insert(Node** head, GenericData data) {
    Node* node = create_node(data);
    node->next = *head;
    *head = node;
}

void print_list(Node* head) {
    Node* cur = head;
    while (cur) {
        cur->data.print_data(cur->data.data);
        printf(" -> ");
        cur = cur->next;
    }
    printf("NULL\n");
}

void delete_node(Node** head, void* key) {
    if (head == NULL || *head == NULL) return;

    Node* temp = *head;
    Node* prev = NULL;

    if (temp->data.compare_data(temp->data.data, key) == 0) {
        *head = temp->next;
        temp->data.free_data(temp->data.data);
        free(temp);
        temp = NULL;
        return;
    }
    while (temp && temp->data.compare_data(temp->data.data, key) != 0) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL) return;
    prev->next = temp->next;
    temp->data.free_data(temp->data.data);
    free(temp);
    temp == NULL;
}

void free_list(Node* head) {
    Node* cur = head;
    Node* next = NULL;

    while (cur) {
        next = cur->next;
        cur->data.free_data(cur->data.data);
        free(cur);
        cur = next;
    }
}