#include <stdio.h>
#include <thread>
#include <conio.h>
#include <condition_variable>
#include <random>
#include <chrono>
#include <cassert>

const int count = 40;           // Number of buffer
int ready_to_consume = 0;       // Number of buffer ready to be consumed
int free_to_produce = count;    // Number of buffer ready for production
int consumer_idx = -1;          // buffer idx to consume
int producer_idx = -1;          // buffer idx to produce
std::mutex mutex;
std::condition_variable consumer_cond;
std::condition_variable producer_cond;

const int cJobDurationMin = 100;      // Simulate work time
const int cJobDurationMax = 300;      // Simulate work time
float consumer_relative_speed = 1.0f; // > 1.0 consumer tends to be faster than producer

std::atomic_int32_t stop_consumer;

// Visual debugging of ring buffer
// p : Producing
// P : Produced
// c : Consuming
// C : Consumed
// x : not initialized
char buffer_state[count+1];


void consumer_thread()
{
    std::random_device rd;      //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd());     //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(cJobDurationMin, cJobDurationMax);

    while(stop_consumer == 0)
    {
        // Wait for something to consume
        std::unique_lock<std::mutex> lLock(mutex);
        consumer_cond.wait(lLock, []() { return ready_to_consume != 0; });
        --ready_to_consume;
        consumer_idx = (consumer_idx + 1) % count;
        buffer_state[consumer_idx] = 'c';
        printf("[%s]\n", buffer_state);
        lLock.unlock();		

        // consume
        int jobTime = distrib(gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(jobTime));

        // Signal consumed
        lLock.lock();
        buffer_state[consumer_idx] = 'C';
        printf("[%s]\n", buffer_state);
        ++free_to_produce;
        producer_cond.notify_one();
        lLock.unlock();
    }
}

void producer()
{
    static std::random_device rd;  //Will be used to obtain a seed for the random number engine
    static std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(int(cJobDurationMin*consumer_relative_speed), int(cJobDurationMax*consumer_relative_speed));

    // Wait for free buffer
    std::unique_lock<std::mutex> lLock(mutex);
    producer_cond.wait(lLock, []() { return free_to_produce != 0; });
    --free_to_produce;
    producer_idx = (producer_idx + 1) % count;	
    buffer_state[producer_idx] = 'p';
    printf("[%s]\n", buffer_state);
    lLock.unlock();	

    // produce
    int jobTime = distrib(gen);
    std::this_thread::sleep_for(std::chrono::milliseconds(jobTime));

    // Signal produced
    lLock.lock();
    buffer_state[producer_idx] = 'P';
    printf("[%s]\n", buffer_state);
    ++ready_to_consume;
    consumer_cond.notify_one();
    lLock.unlock();
}

#define KEY_ESC 27
int main()
{	
    memset(buffer_state, 'x', count);
    buffer_state[count] = '\0';
    std::thread consumer_th = std::thread(consumer_thread);
    int key = 0;

    // Producer loop
    while
        (key != KEY_ESC)
    {
        key = 0;
        if(_kbhit())
             key = _getch();

        float old_consumer_relative_speed = consumer_relative_speed;
        if (key == '+')
            consumer_relative_speed = std::min(consumer_relative_speed + 0.1f, 2.0f);
        if (key == '-')
            consumer_relative_speed = std::max(consumer_relative_speed - 0.1f, 0.0f);

        if (old_consumer_relative_speed != consumer_relative_speed)
        {
            printf("comsumer %f faster than consumer\n", consumer_relative_speed);
        }

        producer();
    }

    stop_consumer.store(1);
    consumer_th.join();
}
