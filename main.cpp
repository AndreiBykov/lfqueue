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
        std::shared_ptr<node> next;

        node(T const &_data) : data(std::make_shared<T>(_data)) {}

        node() {}
    };

    std::shared_ptr<node> head;
    std::shared_ptr<node> tail;

    const unsigned MIN_DELAY = 1;
    const unsigned MAX_DELAY = 100;
    const unsigned FACTOR = 2;

public:
    lfqueue() {
        std::shared_ptr<node> empty_node = std::make_shared<node>();
        empty_node->next = nullptr;
        head = empty_node;
        tail = empty_node;
    }

    void push(T const &data) {
        std::shared_ptr<node> const new_node = std::make_shared<node>(data);
        new_node->next = nullptr;

        std::shared_ptr<node> cur_tail;

        int delay = MIN_DELAY;

        while (true) {

            cur_tail = std::atomic_load(&tail);
            std::shared_ptr<node> cur_next = std::atomic_load(&cur_tail->next);

            if (cur_tail == std::atomic_load(&tail)) {
                if (cur_next == nullptr) {
                    if (std::atomic_compare_exchange_weak(&cur_tail->next, &cur_next, new_node)) {
                        break;
                    }
                } else {
                    std::atomic_compare_exchange_weak(&tail, &cur_tail, cur_next);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            delay = std::min(delay * FACTOR, MAX_DELAY);
        }
        std::atomic_compare_exchange_strong(&tail, &cur_tail, new_node);
    }

    std::shared_ptr<T> pop() {
        std::shared_ptr<T> result;
        std::shared_ptr<node> cur_head;

        int delay = MIN_DELAY;

        while (true) {
            cur_head = std::atomic_load(&head);
            std::shared_ptr<node> cur_tail = std::atomic_load(&tail);
            std::shared_ptr<node> cur_next = std::atomic_load(&cur_head->next);

            if (cur_head == std::atomic_load(&head)) {
                if (head == tail) {
                    if (cur_next == nullptr) {
                        return std::shared_ptr<T>();
                    }
                    std::atomic_compare_exchange_weak(&tail, &cur_tail, cur_next);
                } else {
                    result = cur_next->data;
                    if (std::atomic_compare_exchange_weak(&head, &cur_head, cur_next)) {
                        break;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            delay = std::min(delay * FACTOR, MAX_DELAY);
        }

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