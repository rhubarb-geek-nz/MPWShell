// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using System;
using System.Collections.Generic;
using System.Management.Automation;
using System.Management.Automation.Host;
using System.Management.Automation.Runspaces;
using System.Reflection;

namespace RhubarbGeekNz.MPWShell.ToolServer
{
    sealed internal class ServerState
    {
        internal readonly PSHost host;
        internal readonly Runspace runSpace;
        private readonly DataCapture dataCapture;
        private readonly bool isDebug;
        internal readonly ServerThread serverThread;
        internal readonly static MethodInfo createMethod = typeof(PowerShell).GetMethod("Create", BindingFlags.Static | BindingFlags.Public, null, new Type[] { typeof(Runspace) }, null);
        private CommandInfo outString;
        private object[] createArgs;
        private PowerShell powerShell;
        private IList<string> scriptList = new List<string>();
        internal readonly int clientId;

        internal ServerState(ServerThread serverThread, PSHost host, Runspace runSpace, DataCapture dataCapture, bool isDebug)
        {
            this.serverThread = serverThread;
            this.host = host;
            this.runSpace = runSpace;
            this.dataCapture = dataCapture;
            this.clientId = dataCapture.clientId;
            this.isDebug = isDebug;
            createArgs = new object[] { runSpace };
        }

        internal void Open()
        {
            runSpace.Open();

            PowerShell shell = Create(runSpace);

            try
            {
                MPWShellHost mpwShell = new MPWShellHost(this);

                shell.AddCommand("Get-Command")
                         .AddParameter("Name", "Out-String")
                     .AddStatement()
                         .AddCommand("Set-Variable")
                             .AddParameter("Name", "MPW Shell")
                             .AddParameter("Value", mpwShell);

                var result = shell.Invoke();
                outString = result[0].BaseObject as CommandInfo;
            }
            finally
            {
                shell.Dispose();
            }
        }

        private PowerShell Create(Runspace runSpace)
        {
            PowerShell shell;

            if (createMethod != null)
            {
                shell = createMethod.Invoke(null, createArgs) as PowerShell;
            }
            else
            {
                shell = PowerShell.Create();
                shell.Runspace = runSpace;
            }

            return shell;
        }

        internal void StartScript(string script)
        {
            if (powerShell == null)
            {
                if (script == null)
                {
                    serverThread.serverStates.Remove(runSpace.Id);

                    if (runSpace.RunspaceStateInfo.State == RunspaceState.Opened)
                    {
                        runSpace.Close();
                    }

                    runSpace.Dispose();

                    dataCapture.WriteString(0x42, runSpace.Name);
                }
                else
                {
                    if (createMethod != null)
                    {
                        powerShell = createMethod.Invoke(null, createArgs) as PowerShell;
                    }
                    else
                    {
                        powerShell = PowerShell.Create();
                        powerShell.Runspace = runSpace;
                    }

                    powerShell.AddScript(script).AddCommand(outString).AddParameter("Stream");

                    PSDataCollection<PSObject> input = new PSDataCollection<PSObject>();
                    input.Complete();

                    PSDataCollection<PSObject> output = new PSDataCollection<PSObject>();

                    output.DataAdded += dataCapture.DataAdded;

                    powerShell.Streams.Debug.DataAdded += dataCapture.DebugAdded<DebugRecord>; ;
                    powerShell.Streams.Warning.DataAdded += dataCapture.WarningAdded<WarningRecord>;
                    powerShell.Streams.Error.DataAdded += dataCapture.ErrorAdded;
                    powerShell.Streams.Information.DataAdded += dataCapture.InformationAdded<InformationRecord>;
                    powerShell.Streams.Verbose.DataAdded += dataCapture.VerboseAdded<VerboseRecord>;

                    var task = powerShell.BeginInvoke(input, output);

                    serverThread.Queue(task, EndInvoke);
                }
            }
            else
            {
                scriptList.Add(script);
            }
        }

        internal void EndInvoke(IAsyncResult result)
        {
            PowerShell shell = powerShell;

            powerShell = null;

            if (shell != null)
            {
                try
                {
                    shell.EndInvoke(result);
                }
                catch (ActionPreferenceStopException ex)
                {
                    if (ex.ErrorRecord != null)
                    {
                        dataCapture.WriteError(ex.ErrorRecord);
                    }
                    else
                    {
                        dataCapture.WriteString(0x43,"Exception: " + ex.Message);
                    }
                }
                catch (Exception ex)
                {
                    dataCapture.WriteString(0x44, "Exception: " + ex.Message);
                }
                finally
                {
                    shell.Dispose();

                    dataCapture.WriteString(0x45, "OK");
                }
            }

            if (scriptList.Count > 0)
            {
                string script = scriptList[0];
                scriptList.RemoveAt(0);
                StartScript(script);
            }
        }

        internal void Interrupt(string script)
        {
            PowerShell shell = powerShell;

            if (shell != null)
            {
                try
                {
                    var task = shell.BeginStop((IAsyncResult ar) => { }, this);

                    serverThread.Queue(task, (IAsyncResult ar) => {
                        try
                        {
                            shell.EndStop(ar);
                        }
                        catch (Exception ex)
                        {
                            dataCapture.WriteString(0x44, ex.Message);
                        }
                    });
                }
                catch (Exception ex)
                {
                    dataCapture.WriteString(0x44, ex.Message);
                }
            }
        }

        internal void NewRuntime(int msgType, string content)
        {
            ServerHost host = new ServerHost();

            Runspace runSpace = RunspaceFactory.CreateRunspace(host);

            DataCapture dataCapture = this.dataCapture.Clone(runSpace.Id);

            ServerState state = new ServerState(serverThread, host, runSpace, dataCapture, isDebug);

            host.serverState = state;

            serverThread.serverStates[runSpace.Id] = state;

            dataCapture.WriteString(msgType, content);
        }

        internal void Alert(string message)
        {
            dataCapture.WriteString(0x49, message);
        }
    }
}
