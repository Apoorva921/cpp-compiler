extern "C" int printf(char* fmt, ...);

class Node {
public:
    int val;
    Node* next;

    Node(int v) {
        val = v;
        next = nullptr;
    }
};

class LinkedList {
public:
    Node* head;

    LinkedList() {
        head = nullptr;
    }

    void append(int v) {
        Node* newNode = new Node(v);
        if (head == nullptr) {
            head = newNode;
            return;
        }

        Node* curr = head;
        while (curr->next != nullptr) {
            curr = curr->next;
        }
        curr->next = newNode;
    }

    int length() {
        int count = 0;
        Node* curr = head;
        while (curr != nullptr) {
            count++;
            curr = curr->next;
        }
        return count;
    }

    int sum() {
        int total = 0;
        Node* curr = head;
        while (curr != nullptr) {
            total += curr->val;
            curr = curr->next;
        }
        return total;
    }

    void printList() {
        printf("List contents: ");
        Node* curr = head;
        while (curr != nullptr) {
            printf("%d -> ", curr->val);
            curr = curr->next;
        }
        printf("NULL\n");
    }
};

int main() {
    printf("=== Dynamic Linked List Test ===\n");

    LinkedList* list = new LinkedList();
    list->append(10);
    list->append(20);
    list->append(30);
    list->append(40);
    list->append(50);

    list->printList();
    printf("List length: %d\n", list->length());
    printf("List sum:    %d\n", list->sum());

    return 0;
}
