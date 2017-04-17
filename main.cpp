#include <iostream>
#include <atomic>
#include <thread>

template<typename T>
class lfqueue {

private:

    struct node {
        std::shared_ptr<T> data;
        std::atomic<node *> next;
    };

    std::atomic<node *> head{nullptr};
    std::atomic<node *> tail{nullptr};

    std::atomic<unsigned> threads_in_pop;
    std::atomic<node *> delete_list;

    const unsigned MIN_DELAY = 1;
    const unsigned MAX_DELAY = 100;
    const unsigned FACTOR = 2;

    static void delete_nodes(node *nodes) {
        while (nodes) {
            node *next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }

    void try_reclaim(node *old_head) {
        if (threads_in_pop == 1) {
            node *nodes_to_delete = delete_list.exchange(nullptr);
            if (!--threads_in_pop) {
                delete_nodes(nodes_to_delete);
            } else if (nodes_to_delete) {
                chain_pending_nodes(nodes_to_delete);
            }
            delete old_head;
        } else {
            chain_pending_node(old_head);
            --threads_in_pop;
        }
    }

    void chain_pending_nodes(node *nodes) {
        node *last = nodes;
        while (node *const next = last->next)
            last = next;
        chain_pending_nodes(nodes, last);
    }

    void chain_pending_nodes(node *first, node *last) {
        node *next;
        do {
            next = delete_list;
            last->next = next;
        } while (delete_list.compare_exchange_weak(next, first));
    }

    void chain_pending_node(node *n) {
        chain_pending_nodes(n, n);
    }

public:
    lfqueue() {
        node *empty_node = new node();
        empty_node->next = nullptr;
        head.store(empty_node);
        tail.store(empty_node);
    }

    void push(T const &data) {
        node *new_node = new node();
        new_node->data = std::make_shared<T>(data);
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

    std::shared_ptr<T> pop() {
        threads_in_pop++;
        node *cur_head;

        int delay = MIN_DELAY;

        std::shared_ptr<T> result;
        while (true) {
            cur_head = head.load();
            node *cur_tail = tail.load();
            node *cur_next = cur_head->next.load();

            if (cur_head == head.load()) {
                if (head == tail) {
                    if (cur_next == nullptr) {
                        return std::shared_ptr<T>();
                    }
                    tail.compare_exchange_weak(cur_tail, cur_next);
                } else {
                    result.swap(cur_next->data);
                    if (head.compare_exchange_weak(cur_head, cur_next)) {
                        break;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            delay = std::min(delay * FACTOR, MAX_DELAY);
        }

        try_reclaim(cur_head);
        return result;
    }

};

int main() {

    lfqueue<int> *lfqueue1 = new lfqueue<int>();
    for (int i = 0; i < 10; i++)
        lfqueue1->push(i);
    std::shared_ptr<int> res;
    for (int j = 0; j < 20; ++j) {
        res = lfqueue1->pop();
        if (res) {
            std::cout << *res.get() << std::endl;
        } else {
            std::cout << "Queue is empty" << std::endl;
        }
    }

    return 0;
}