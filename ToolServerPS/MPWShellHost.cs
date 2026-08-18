// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using System;

namespace RhubarbGeekNz.MPWShell.ToolServer
{
    sealed internal class MPWShellHost : IMPWShell
    {
        private readonly ServerState serverState;

        internal MPWShellHost(ServerState serverState)
        {
            this.serverState = serverState;
        }

        public void New(string content = null)
        {
            if (content == null)
            {
                content = String.Empty;
            }

            serverState.serverThread.Queue((IAsyncResult) => serverState.NewRuntime(0x47, content));
        }

        public void Open(string filePath)
        {
            if (filePath == null)
            {
                throw new NullReferenceException();
            }

            serverState.serverThread.Queue((IAsyncResult) => serverState.NewRuntime(0x48, filePath));
        }

        public void Alert(string message)
        {
            if (message == null)
            {
                throw new NullReferenceException();
            }

            serverState.Alert(message);
        }

        public override string ToString()
        {
            return "MPW Shell";
        }
    }
}
