// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

static int
L1compare(Thread *t1, Thread *t2)
{
    if (t1->getRemainingBurstTime() > t2->getRemainingBurstTime())
        return 1;
    else if (t1->getRemainingBurstTime() < t2->getRemainingBurstTime())
        return -1;
    else if (t1->getID() > t2->getID())
        return 1;
    else if (t1->getID() < t2->getID())
        return -1;
    else
        return 0;
}
static int
L2compare(Thread *t1, Thread *t2)
{

    if (t1->getID() > t2->getID())
        return 1;
    else if (t1->getID() < t2->getID())
        return -1;
    else
        return 0;
}
//<TODO done>

Scheduler::Scheduler()
{
    //schedulerType = type;
    //<TODO>
    L1ReadyQueue = new SortedList<Thread *>(L1compare);
    L2ReadyQueue = new SortedList<Thread *>(L2compare);
    L3ReadyQueue = new List<Thread *>;
    toBeDestroyed = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{
    //<TODO done>
    delete L1ReadyQueue;
    delete L2ReadyQueue;
    delete L3ReadyQueue;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());

    Statistics *stats = kernel->stats;
    thread->setStatus(READY);

    if (thread->getPriority() >= 100 && thread->getPriority() < 150)
    {
	if (kernel->currentThread->getPriority()<100 || kernel->currentThread->getRemainingBurstTime() > thread->getRemainingBurstTime())
	    kernel->interrupt->YieldOnReturn();
        if (!L1ReadyQueue->IsInList(thread))
        {
            L1ReadyQueue->Insert(thread);
            DEBUG('z', "[InsertToQueue] Tick [" << kernel->stats->totalTicks << "] Thread [" << thread->getID() << "] is inserted into queue L[1]");
        }
    }
    else if (thread->getPriority() >= 50 && thread->getPriority() < 100)
    {
        if (!L2ReadyQueue->IsInList(thread))
        {
            L2ReadyQueue->Insert(thread);
            DEBUG('z', "[InsertToQueue] Tick [" << kernel->stats->totalTicks << "] Thread [" << thread->getID() << "] is inserted into queue L[2]");
        }
    }
    else
    {
        if (!L3ReadyQueue->IsInList(thread))
        {
            L3ReadyQueue->Append(thread);
            DEBUG('z', "[InsertToQueue] Tick [" << kernel->stats->totalTicks << "] Thread [" << thread->getID() << "] is inserted into queue L[3]");
        }
    }
    //<todo done>
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    if (!L1ReadyQueue->IsEmpty())
    {
        Thread *t = L1ReadyQueue->RemoveFront();
        DEBUG('z', "[RemoveFromQueue] Tick [" << kernel->stats->totalTicks << "] Thread [" << t->getID() << "] is removed from queue L[1]");
        return t;
    }
    else if (!L2ReadyQueue->IsEmpty())
    {
        Thread *t = L2ReadyQueue->RemoveFront();
        DEBUG('z', "[RemoveFromQueue] Tick [" << kernel->stats->totalTicks << "] Thread [" << t->getID() << "] is removed from queue L[2]");
        return t;
    }
    else if (!L3ReadyQueue->IsEmpty())
    {
        Thread *t = L3ReadyQueue->RemoveFront();
        DEBUG('z', "[RemoveFromQueue] Tick [" << kernel->stats->totalTicks << "] Thread [" << t->getID() << "] is removed from queue L[3]");
        return t;
    }
    else
        return NULL;
    //<TODO done>
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;

    //	cout << "Current Thread" <<oldThread->getName() << "    Next Thread"<<nextThread->getName()<<endl;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing)
    { // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

#ifdef USER_PROGRAM // ignore until running user programs
    if (oldThread->space != NULL)
    { // if this thread is a user program,

        oldThread->SaveUserState(); // save the user's CPU registers
        oldThread->space->SaveState();
    }
#endif

    oldThread->CheckOverflow(); // check if the old thread
                                // had an undetected stack overflow

    kernel->currentThread = nextThread; // switch to the next thread
    nextThread->setStatus(RUNNING);     // nextThread is now running

    // DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    cout << "Switching from: " << oldThread->getID() << " to: " << nextThread->getID() << endl;
    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << kernel->currentThread->getID());

    CheckToBeDestroyed(); // check if thread we were running
                          // before this one has finished
                          // and needs to be cleaned up

#ifdef USER_PROGRAM
    if (oldThread->space != NULL)
    {                                  // if there is an address space
        oldThread->RestoreUserState(); // to restore, do it.
        oldThread->space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL)
    {
        DEBUG(dbgThread, "toBeDestroyed->getID(): " << toBeDestroyed->getID());
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
//  Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print()
{
    cout << "Ready list contents:\n";
    L1ReadyQueue->Apply(ThreadPrint);
    L2ReadyQueue->Apply(ThreadPrint);
    L3ReadyQueue->Apply(ThreadPrint);
}

// <TODO done>

void Scheduler::UpdatePriority()
{
    ListIterator<Thread *> *iter1 = new ListIterator<Thread *>(L1ReadyQueue);
    ListIterator<Thread *> *iter2 = new ListIterator<Thread *>(L2ReadyQueue);
    ListIterator<Thread *> *iter3 = new ListIterator<Thread *>(L3ReadyQueue);

    int new_priority, old_priority;
    for (; !iter1->IsDone(); iter1->Next())
    {
        iter1->Item()->setWaitTime(iter1->Item()->getWaitTime() + TimerTicks);
        if (iter1->Item()->getWaitTime() > 400 && iter1->Item()->getID() > 0)
        {
            old_priority = iter1->Item()->getPriority();
            new_priority = iter1->Item()->getPriority() + 10;
            if (new_priority > 149)
                new_priority = 149;
            iter1->Item()->setPriority(new_priority);
            DEBUG('z', "[UpdatePriority] Tick [" << kernel->stats->totalTicks << "] Thread [" << iter1->Item()->getID() << "] changes its priority from[" << old_priority << "] to [" << new_priority << "]");
            iter1->Item()->setWaitTime(0);
        }
    }

    for (; !iter2->IsDone(); iter2->Next())
    {
        iter2->Item()->setWaitTime(iter2->Item()->getWaitTime() + TimerTicks);
        if (iter2->Item()->getWaitTime() > 400 && iter2->Item()->getID() > 0)
        {
            old_priority = iter2->Item()->getPriority();
            new_priority = iter2->Item()->getPriority() + 10;
            if (new_priority > 149)
                new_priority = 149;
            iter2->Item()->setPriority(new_priority);
            iter2->Item()->setWaitTime(0);
            DEBUG('z', "[UpdatePriority] Tick [" << kernel->stats->totalTicks << "] Thread [" << iter2->Item()->getID() << "] changes its priority from[" << old_priority << "] to [" << new_priority << "]");
            L2ReadyQueue->Remove(iter2->Item());
            ReadyToRun(iter2->Item());
        }
    }

    for (; !iter3->IsDone(); iter3->Next())
    {
        iter3->Item()->setWaitTime(iter3->Item()->getWaitTime() + TimerTicks);
        if (iter3->Item()->getWaitTime() > 400 && iter3->Item()->getID() > 0)
        {
            old_priority = iter3->Item()->getPriority();
            new_priority = iter3->Item()->getPriority() + 10;
            if (new_priority > 149)
                new_priority = 149;
            DEBUG('z', "[UpdatePriority] Tick [" << kernel->stats->totalTicks << "] Thread [" << iter3->Item()->getID() << "] changes its priority from[" << old_priority << "] to [" << new_priority << "]");
            iter3->Item()->setPriority(new_priority);
            iter3->Item()->setWaitTime(0);
            L3ReadyQueue->Remove(iter3->Item());
            ReadyToRun(iter3->Item());
        }
    }
}
