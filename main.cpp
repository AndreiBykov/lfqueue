#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

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
                        return false;
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

void thread_func(lfqueue<int> *lfq, int amount_of_operations) {

    unsigned seed = (unsigned) time(NULL);
    for (int i = 0; i < amount_of_operations; ++i) {
        switch (rand_r(&seed) % 2) {
            case 0:
                lfq->push(rand_r(&seed));
                break;
            case 1:
                lfq->pop();
                break;

            default:
                break;
        }
    }

}

int main(int argc, char **argv) {

    if (argc < 3) {
        std::cout << "Not enought args" << std::endl;
    }

    int amount_of_threads = atoi(argv[1]);
    int amount_of_operations = atoi(argv[2]);

    lfqueue<int> *lfq = new lfqueue<int>();

    std::vector<std::thread> threads;

    auto get_time = std::chrono::steady_clock::now;
    decltype(get_time()) start, end;
    start = get_time();

    for (int i = 0; i < amount_of_threads; ++i) {
        int amount = amount_of_operations / (amount_of_threads - i);
        amount_of_operations -= amount;
        threads.push_back(std::thread(thread_func, lfq, amount));
    }

    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    end = get_time();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Elapsed time: " << double(elapsed) / 1000 << " s\n";
    return 0;
}