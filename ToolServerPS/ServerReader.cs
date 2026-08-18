// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using System;
using System.IO;
using System.Linq;
using System.Management.Automation.Runspaces;
using System.Text;
using System.Threading;

namespace RhubarbGeekNz.MPWShell.ToolServer
{
    sealed public class ServerReader
    {
        readonly Stream inputStream, outputStream;
        readonly bool isDebugEnabled;
        readonly object outputStreamMutex = new object();
        static readonly Guid protocol = Guid.Parse("0c63fba6-7c2a-4b72-8de0-b3bd579dedaa");

        public ServerReader(Stream input,Stream output, Guid protocol, bool isDebugEnabled)
        {
            this.inputStream = input;
            this.outputStream = output;
            this.isDebugEnabled = isDebugEnabled;

            if (!ServerReader.protocol.Equals(protocol))
            {
                throw new System.ArgumentException("Unsupported protocol");
            }
        }

        static readonly string magicPrefix = protocol + Environment.NewLine;

        public void Invoke()
        {
            byte[] prefix = Encoding.UTF8.GetBytes(magicPrefix);

            outputStream.Write(prefix, 0, prefix.Length);

            ServerThread serverThread = new ServerThread();

            Thread thread = new Thread(serverThread.ThreadTask);

            thread.Start();

            try
            {
                byte[] headers = new byte[16];

                while (true)
                {
                    int i = 0;

                    while (i < headers.Length)
                    {
                        int j = inputStream.Read(headers, i, headers.Length - i);
                        if (j < 1) break;
                        i += j;
                    }

                    if (i != headers.Length)
                    {
                        break;
                    }

                    int packetType = BitConverter.ToInt32 (headers, 0);
                    int clientId = BitConverter.ToInt32(headers, 4);
                    int runspaceId = BitConverter.ToInt32(headers, 8);
                    int dataLen = BitConverter.ToInt32(headers, 12);
                    string str = null;

                    if (dataLen > 0)
                    {
                        byte[] b= new byte[dataLen];
                        i = 0;
                        while (i < dataLen)
                        {
                            int j=inputStream.Read(b,i, dataLen - i);
                            if (j > 0)
                            {
                                i += j;
                            }
                            else
                            {
                                break;
                            }
                        }

                        if (i != dataLen)
                        {
                            break;
                        }

                        str = Encoding.UTF8.GetString(b);
                    }

                    serverThread.Queue((IAsyncResult) => {
                        HandleScript(serverThread, packetType, clientId, runspaceId, str);
                    });
                }
            }
            finally
            {
                serverThread.Queue((IAsyncResult) => { serverThread.IsRunning = false; });
                thread.Join();
            }
        }

        private void HandleScript(ServerThread serverThread, int packetType, int clientId, int runspaceId, string script)
        {
            try
            {
                switch (packetType)
                {
                    case 0x80:
                        {
                            DataCapture dataCapture = new DataCapture(outputStream, outputStreamMutex, clientId, runspaceId, isDebugEnabled);

                            dataCapture.WriteString(0x81, script);
                        }
                        break;

                    case 0x50:
                        {
                            if (serverThread.serverStates.TryGetValue(runspaceId, out var state))
                            {
                                if (state.clientId == clientId)
                                {
                                    state.StartScript(script);
                                }
                            }
                        }
                        break;

                    case 0x51:
                        {
                            ServerHost host = new ServerHost();

                            Runspace runSpace = RunspaceFactory.CreateRunspace(host);

                            DataCapture dataCapture = new DataCapture(outputStream, outputStreamMutex, clientId, runSpace.Id, isDebugEnabled);

                            ServerState state = new ServerState(serverThread, host, runSpace, dataCapture, isDebugEnabled);

                            host.serverState = state;

                            state.Open();

                            serverThread.serverStates[runSpace.Id] = state;

                            dataCapture.WriteString(0x41, runSpace.Name);

                            if (script != null)
                            {
                                state.StartScript(script);
                            }
                        }
                        break;

                    case 0x52:
                        {
                            if (serverThread.serverStates.TryGetValue(runspaceId, out var state))
                            {
                                if (clientId == state.clientId)
                                {
                                    state.StartScript(null);
                                }
                            }
                        }
                        break;

                    case 0x53:
                        {
                            var list = serverThread.serverStates.Values.Where(s => s.clientId == clientId).ToArray();

                            foreach (ServerState state in list)
                            {
                                state.StartScript(null);
                            }
                        }
                        break;

                    case 0x54:
                        {
                            if (serverThread.serverStates.TryGetValue(runspaceId, out var state))
                            {
                                if (state.clientId == clientId)
                                {
                                    state.Interrupt(script);
                                }
                            }
                        }
                        break;

                    case 0x55:
                        {
                            if (serverThread.serverStates.TryGetValue(runspaceId, out var state))
                            {
                                if ((clientId != state.clientId)
                                    &&
                                    (state.runSpace.RunspaceStateInfo.State == RunspaceState.BeforeOpen))
                                {
                                    DataCapture dataCapture = new DataCapture(outputStream, outputStreamMutex, clientId, runspaceId, isDebugEnabled);

                                    ServerState newState = new ServerState(state.serverThread, state.host, state.runSpace, dataCapture, isDebugEnabled);

                                    ServerHost host = newState.host as ServerHost;

                                    host.serverState = newState;

                                    serverThread.serverStates[runspaceId] = newState;

                                    newState.Open();

                                    if (script != null)
                                    {
                                        newState.StartScript(script);
                                    }
                                }
                            }
                        }
                        break;

                    default:
                        {
                            DataCapture dataCapture = new DataCapture(outputStream, outputStreamMutex, clientId, -1, isDebugEnabled);

                            dataCapture.WriteString(0x44, $"{packetType},{clientId},{runspaceId},{script}");
                        }
                        break;
                }

            }
            catch (Exception ex)
            {
                DataCapture dataCapture = new DataCapture(outputStream, outputStreamMutex, clientId, runspaceId, isDebugEnabled);

                dataCapture.WriteString(0x43, ex.Message);
            }
        }
    }
}
