// Server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <mutex>
#include <queue>

struct Order {};

class MarketSynchronization {



    bool IsOrderValid(const Order& order) {
        return true;
    }


    std::queue<Order>* orderQueue = new std::queue<Order>();
    std::mutex orderQueueMutex = std::mutex();
public:
    void ExecuteQueue() {
        std::queue<Order>* currentQueue = nullptr;
        {
            std::lock_guard lock = std::lock_guard(orderQueueMutex);
            currentQueue = orderQueue;
            orderQueue = new std::queue<Order>();
        }
        if (currentQueue == nullptr) {
            throw std::exception("Failed getting `currentQueue`.");
        }


        delete currentQueue;
    }

    bool SubmitOrder(Order& order) {
        if (!IsOrderValid(order)) {
            return false;
        }

        {
            std::lock_guard lock = std::lock_guard(orderQueueMutex);
            orderQueue->push(order);
        }
    }
};


void loop_add_orders() {}

void loop_market_step() {}

int main()
{
    std::cout << "Hello World!\n";
}
