//open source minimal invasion udp messager for local netowrk stuff
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <boost/asio.hpp>
#include <iostream>
#include <regex>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <future>
#include <queue>
#include <functional>

class ThreadPool {
    //I have no idea how threading works
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    void workerThread();
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&ThreadPool::workerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    stop.store(true);
    condition.notify_all();
    for (std::thread &worker : workers) {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.push(std::move(task));
    }
    condition.notify_one();
}

void ThreadPool::workerThread() {
    while (!stop.load()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop.load() || !tasks.empty(); });
            if (stop.load() && tasks.empty()) {
                return;
            }
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

ThreadPool threadPool(4);
using boost::asio::ip::udp;



void sender(std::string targetaddresstring, std::string whoistalking, int targetport) {
	boost::asio::io_context io_context;
	
    //create and open UDP socket
    udp::socket socket(io_context);
    socket.open(udp::v4());

    udp::endpoint server_endpoint;

    try {
        //create IP address object from a string
        boost::asio::ip::address ip_address = boost::asio::ip::make_address(targetaddresstring);

        //create UDP endpoint using the IP address and a port number
        server_endpoint = udp::endpoint(ip_address, targetport);

        std::cout << "Endpoint created: " << server_endpoint << std::endl;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Error creating endpoint: " << e.what() << std::endl;
        return; //exit if error
    }
    std::string messagehead = "[" + whoistalking + "]: ";
	while (true) {
	    //message to send
	    std::cout << messagehead;
	    std::string message;
	    std::getline(std::cin, message);
	    std::string fullmessage = messagehead+message;
	    try {
	        //attempt to send message
	        size_t bytes_sent = socket.send_to(boost::asio::buffer(fullmessage), server_endpoint);
	    } catch (const boost::system::system_error& e) {
	        std::cerr << "Error sending message: " << e.what() << std::endl;
	        return; //exit if error
	    }
	}
	
}

void receiver(int targetport) {
	boost::asio::io_context io_context;
	
    //create a UDP socket
    udp::socket socket(io_context, udp::endpoint(udp::v4(), targetport));

    while (true) {
        std::array<char, 1024> recv_buffer;
        udp::endpoint remote_endpoint;
        boost::system::error_code error;

        //receive a message
        size_t bytes_received = socket.receive_from(boost::asio::buffer(recv_buffer), remote_endpoint, 0, error);

        if (error) {
            std::cerr << "Receive error: " << error.message() << std::endl;
            continue;
        }

        //construct a string from the received data
        std::string received_data(recv_buffer.data(), bytes_received);
        std::cout << received_data << std::endl;
    }
}


int main() {
    //just inits stuff
	std::string targetipaddress;
	int targetport;
	std::string whoistalking;
	std::cout << "You started cluncmsg, remember this is definitely not encrypted :D" << std::endl;
	std::cout << "Please enter who is talking:" << std::endl;
	std::getline(std::cin, whoistalking);
	std::cout << "please enter the ip address of who you want to talk to:" << std::endl;
	std::getline(std::cin, targetipaddress);
	std::cout << "pleast enter the port which you want to communicate on" << std::endl;
	std::cin >> targetport;
	threadPool.enqueue([=]() {receiver(targetport);});
	threadPool.enqueue([=]() {sender(targetipaddress, whoistalking, targetport);});
    return 0;
}
