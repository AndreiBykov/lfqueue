#include <iostream>
#include <atomic>
#include <thread>

template<typename T>
class lfqueue {

private:

    struct node {
        std::shared_ptr<T> data;
        std::shared_ptr<node> next;

        node(T const &_data) : data(std::make_shared<T>(_data)) {}
    };

    std::shared_ptr<node> head;
    std::shared_ptr<node> tail;

    const unsigned MIN_DELAY = 1;
    const unsigned MAX_DELAY = 100;
    const unsigned FACTOR = 2;

public:
    lfqueue(T data) {
        std::shared_ptr<node> empty_node = std::make_shared<node>(data);
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

int main() {

    lfqueue<int> *lfqueue1 = new lfqueue<int>(1010);
    for (int i = 0; i < 10; i++)
        lfqueue1->push(i);

    for (int j = 0; j < 20; ++j) {
        std::shared_ptr<int> result= lfqueue1->pop();
        if (result != nullptr) {
            std::cout << *result.get() << std::endl;
        } else{
            std::cout<<"Queue is empty"<<std::endl;
        }
    }

    return 0;
}