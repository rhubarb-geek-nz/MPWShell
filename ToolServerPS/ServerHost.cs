// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using System;
using System.Globalization;
using System.Management.Automation.Host;

namespace RhubarbGeekNz.MPWShell.ToolServer
{
    sealed internal class ServerHost : PSHost
    {
        private readonly Guid guid = Guid.NewGuid();
        private readonly CultureInfo currentCulture = System.Threading.Thread.CurrentThread.CurrentCulture;
        private readonly CultureInfo currentUICulture = System.Threading.Thread.CurrentThread.CurrentUICulture;
        public override CultureInfo CurrentCulture => currentCulture;
        public override CultureInfo CurrentUICulture => currentUICulture;
        public override Guid InstanceId => guid;
        public override string Name => "MPW Shell";
        public override PSHostUserInterface UI => null;
        public override Version Version => typeof(ServerHost).Assembly.GetName().Version;
        public int ExitCode;
        public bool IsRunning = true;

        internal ServerState serverState;

        public override void EnterNestedPrompt()
        {
        }

        public override void ExitNestedPrompt()
        {
        }

        public override void NotifyBeginApplication()
        {
        }

        public override void NotifyEndApplication()
        {
        }

        public override void SetShouldExit(int exitCode)
        {
            IsRunning = false;
            ExitCode = exitCode;

            serverState.serverThread.Queue((IAsyncResult) => { serverState.StartScript(null); });
        }

        internal ServerHost()
        {
        }
    }
}
