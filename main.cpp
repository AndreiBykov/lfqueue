#include <iostream>
#include <atomic>
#include <thread>

template<typename T>
class lfqueue {

private:

    struct node {
        T data;
        std::atomic<node *> next;
    };

    std::atomic<node *> head{nullptr};
    std::atomic<node *> tail{nullptr};

    const unsigned MIN_DELAY = 1;
    const unsigned MAX_DELAY = 100;
    const unsigned FACTOR = 2;

public:
    lfqueue() {
        node *empty_node = new node();
        empty_node->next = nullptr;
        head.store(empty_node);
        tail.store(empty_node);
    }

    void push(T const &data) {
        node *new_node = new node();
        new_node->data = data;
        new_node->next = nullptr;

        node *cur_tail;

        int delay = MIN_DELAY;

        while (true) {

            cur_tail = tail.load();
            node *cur_next = cur_tail->next.load();

            if (cur_tail == tail.load()) {
                if (cur_next == nullptr) {
                    if (cur_tail->next.compare_exchange_weak(cur_next, new_node)) {
                        break;
                    }
                } else {
                    tail.compare_exchange_weak(cur_tail, cur_next);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            delay = std::min(delay * FACTOR, MAX_DELAY);
        }
        tail.compare_exchange_strong(cur_tail, new_node);
    }

    bool pop(T &result) {
        node *cur_head;

        int delay = MIN_DELAY;

        while (true) {
            cur_head = head.load();
            node *cur_tail = tail.load();
            node *cur_next = cur_head->next.load();

            if (cur_head == head.load()) {
                if (head == tail) {
                    if (cur_next == nullptr) {
                        return false;
                    }
                    tail.compare_exchange_weak(cur_tail, cur_next);
                } else {
                    result = cur_next->data;
                    if (head.compare_exchange_weak(cur_head, cur_next)) {
                        break;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            delay = std::min(delay * FACTOR, MAX_DELAY);
        }
        delete cur_head;

        return true;
    }

};

int main() {

    lfqueue<int> *lfqueue1 = new lfqueue<int>();
    for (int i = 0; i < 10; i++)
        lfqueue1->push(i);
    int res;
    for (int j = 0; j < 20; ++j) {
        if (lfqueue1->pop(res)) {
            std::cout << res << std::endl;
        } else {
            std::cout << "Queue is empty" << std::endl;
        }
    }

    return 0;
}