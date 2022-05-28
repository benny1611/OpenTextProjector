#include <queue>
#include "tinythread.h"

template<typename Data>
class concurrent_queue
{
private:
    std::queue<Data> the_queue;
    mutable tthread::mutex the_mutex;
public:
    void push(const Data& data)
    {
        tthread::lock_guard<tthread::mutex>(this->the_mutex);
        the_queue.push(data);
    }

    bool empty() const
    {
        tthread::lock_guard<tthread::mutex>(this->the_mutex);
        return the_queue.empty();
    }

    Data& front()
    {
        tthread::lock_guard<tthread::mutex>(this->the_mutex);
        return the_queue.front();
    }

    Data const& front() const
    {
        tthread::lock_guard<tthread::mutex>(this->the_mutex);
        return the_queue.front();
    }

    void pop(Data& data)
    {
        tthread::lock_guard<tthread::mutex>(this->the_mutex);
        data = the_queue.front();
        the_queue.pop();
    }
};