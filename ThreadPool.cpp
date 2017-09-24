/**
  ******************************************************************************
  * @file    main.c
  * @author  Oleg Gordiushenkov
  * @version V1.0
  * @date    24-September-2016
  * @brief   Thread pool implementation
  ******************************************************************************
  */

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#define DEBUG_OUT
#ifdef DEBUG_OUT
	#define LOG(x) \
	{\
	std::lock_guard<std::mutex> lock(g_lockCout); \
	x;\
	}
#else
	#define LOG(x)
#endif

enum {
	nEvaluations = 11,	// Number of scheduled evaluations
	nThreads = 4,		// Numver of threads in ThreadPool
};

std::mutex g_lockCout;

class ThreadPool {
	struct ThreadManager; // forward declaration
public:
	// Constructor
	ThreadPool(int nThreads) {
		m_nThreads = nThreads;
		m_vpThreads.reserve(m_nThreads);
		for (int i = 0; i < nThreads; ++i) {
			m_vpThreads.push_back(new ThreadManager(i));
		}
	}
	// Schedule evaluation to a free thread result = f(param1, param2), ready - set to true when evaluation finished
	// returns true if evaluation was succesfully scheduled, otherwise - false
	bool evaluate(int(*f)(int, int), int param1, int param2, int& result, bool& ready) {
		bool scheduled = false;
		for (int i = 0; i < m_nThreads; ++i) {
			if (m_vpThreads.at(i)->free) {
				std::unique_lock<std::mutex> locker(m_vpThreads.at(i)->m_mxQueue);
				m_vpThreads.at(i)->free = false;
				scheduled = true;
				m_vpThreads.at(i)->functionsQueue.push([param1, param2, &result, &ready, f]() -> void {
					result = f(param1, param2);
					ready = true;
				});
				m_vpThreads.at(i)->m_cvNewFunc.notify_one();
				break;
			}
		}
		return scheduled;
	}
private:
	int m_nThreads;
	std::vector<ThreadManager*> m_vpThreads;

	struct ThreadManager {
		std::mutex m_mxQueue;
		std::condition_variable m_cvNewFunc;
		std::queue < std::function<void(void)> > functionsQueue;
		bool free;
		int m_id;

		// Constructor
		ThreadManager(int id) : m_id(id), free(true) {
			std::thread thr(&ThreadPool::ThreadManager::execute, this);
			thr.detach();
		}
		// Thread function
		static void execute(ThreadManager* pMngr) {
			for (;;) {
				std::unique_lock<std::mutex> locker(pMngr->m_mxQueue);
				while (pMngr->functionsQueue.empty()) {
					pMngr->m_cvNewFunc.wait(locker);
				}
				std::this_thread::sleep_for(std::chrono::seconds(1));
				LOG(std::cout << "Starting function in thread " << pMngr->m_id << std::endl);
				std::function<void(void)> f = pMngr->functionsQueue.front();
				f();
				pMngr->functionsQueue.pop();
				pMngr->free = true;
			}
		}

	};
};

int myFunc_1(int a, int b) {
	LOG(std::cout << "Executing myFunc_1" << std::endl);
	return a + b;
}
int myFunc_2(int a, int b) {
	LOG(std::cout << "Executing myFunc_2" << std::endl);
	return a - b;
}
int myFunc_3(int a, int b) {
	LOG(std::cout << "Executing myFunc_3" << std::endl);
	return a * b;
}
int myFunc_4(int a, int b) {
	LOG(std::cout << "Executing myFunc_4" << std::endl);
	return a / b;
}

struct evalStruct {
	int param1;
	int param2;
	int result;
	bool readyFlag;
	int(*func)(int, int);
};

typedef int(*func_t)(int, int);
func_t funcs[] = {
	myFunc_1,
	myFunc_2,
	myFunc_3,
	myFunc_4,
};

static const int nFuncs = sizeof(funcs) / sizeof(func_t);

void dump(std::vector<evalStruct> &vEvals) {
	enum {
		titleWidth = 12,
		valWidth = 4,
	};

	LOG(std::cout << std::setw(titleWidth) << "param1=");
	for (int i = 0; i < nEvaluations; ++i) {
		LOG(std::cout << std::setw(valWidth) << vEvals.at(i).param1);
	}
	LOG(std::cout << std::endl);
	LOG(std::cout << std::setw(titleWidth) << "param2=");
	for (int i = 0; i < nEvaluations; ++i) {
		LOG(std::cout << std::setw(valWidth) << vEvals.at(i).param2);
	}
	LOG(std::cout << std::endl);
	LOG(std::cout << std::setw(titleWidth) << "results=");
	for (int i = 0; i < nEvaluations; ++i) {
		LOG(std::cout << std::setw(valWidth) << vEvals.at(i).result);
	}
	LOG(std::cout << std::endl);
	LOG(std::cout << std::setw(titleWidth) << "readyFlags=");
	for (int i = 0; i < nEvaluations; ++i) {
		LOG(std::cout << std::setw(valWidth) << vEvals.at(i).readyFlag);
	}
	LOG(std::cout << std::endl);
}

int main()
{
	std::vector<evalStruct> vEvals;
	vEvals.reserve(nFuncs);
	for (int i = 0; i < nEvaluations; ++i) {
		vEvals.push_back( { 2 * i, i, 0, false, funcs[i % nFuncs] } );
	}
	
	ThreadPool pool = ThreadPool(nThreads);
	LOG(std::cout << "Initial values:" << std::endl);
	dump(vEvals);

	for (int i = 0; i < nEvaluations; ++i) {
		bool scheduled = false;
		do  {
			scheduled = pool.evaluate(vEvals.at(i).func, vEvals.at(i).param1, vEvals.at(i).param2, vEvals.at(i).result, vEvals.at(i).readyFlag);
		} while (!scheduled);
		LOG(std::cout << "Scheduled " << i << std::endl);
	}

	std::this_thread::sleep_for(std::chrono::seconds(2));

	LOG(std::cout << "Final values:" << std::endl);
	dump(vEvals);
	
	return 0;
}

