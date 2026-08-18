// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using System;
using System.Collections.Generic;
using System.Threading;

namespace RhubarbGeekNz.MPWShell.ToolServer
{
    sealed internal class ServerThread
    {
        private readonly Semaphore mutex = new Semaphore(1, 1);
        volatile internal bool IsRunning = true;
        private AutoResetEvent autoEvent = new AutoResetEvent(false);
        private readonly AlreadyDone alreadyDone = new AlreadyDone();
        internal readonly IDictionary<int,ServerState> serverStates= new Dictionary<int,ServerState>();

        internal IDisposable Acquire()
        {
            mutex.WaitOne();

            return new SemaphoreLock(mutex);
        }

        IList<ServerEvent> eventsList = new List<ServerEvent>();

        internal void ThreadTask()
        {
            while (IsRunning)
            {
                ServerEvent e = null;
                WaitHandle[] list = null;

                using (var m = Acquire())
                {
                    int i = 0;

                    if (list == null || list.Length != eventsList.Count+1)
                    {
                        list = new WaitHandle[eventsList.Count + 1];
                    }

                    foreach (ServerEvent p in eventsList)
                    {
                        if (p.result.IsCompleted)
                        {
                            e = p;
                            break;
                        }

                        list[i++] = p.result.AsyncWaitHandle;
                    }

                    if (e != null)
                    {
                        eventsList.RemoveAt(i);
                    }
                    else
                    {
                        list[i++] = autoEvent;
                    }
                }

                if (e != null)
                {
                    e.handler(e.result);
                }
                else
                {
                    WaitHandle.WaitAny(list);
                }
            }
        }

        internal void Queue(IAsyncResult result, ServerEvent.EventHandler handler)
        {
            using (var m = Acquire())
            {
                eventsList.Add(new ServerEvent(result, handler));
                autoEvent.Set();
            }
        }

        internal void Queue(ServerEvent.EventHandler handler)
        {
            using (var m = Acquire())
            {
                eventsList.Add(new ServerEvent(alreadyDone, handler));
                autoEvent.Set();
            }
        }
    }

    internal class ServerEvent
    {
        internal readonly IAsyncResult result;
        internal delegate void EventHandler(IAsyncResult se);
        internal readonly EventHandler handler;

        internal ServerEvent(IAsyncResult result, EventHandler handler)
        {
            this.result = result;
            this.handler = handler;
        }
    }

    internal class AlreadyDone : IAsyncResult
    {
        public object AsyncState => throw new NotImplementedException();

        public WaitHandle AsyncWaitHandle => throw new NotImplementedException();

        public bool CompletedSynchronously => true;

        public bool IsCompleted => true;
    }

    internal class SemaphoreLock : IDisposable
    {
        private readonly Semaphore sem;

        internal SemaphoreLock(Semaphore sem)
        {
            this.sem = sem;
        }

        public void Dispose()
        {
            sem.Release();
        }
    }
}
