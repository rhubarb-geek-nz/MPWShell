// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using System;
using System.Management.Automation;
using System.Text;

namespace RhubarbGeekNz.MPWShell.ToolServer
{
    [Cmdlet(VerbsLifecycle.Invoke, "MPWShell.ToolServer")]
    sealed public class InvokeToolServer : PSCmdlet
    {
        [Parameter(Position = 0,Mandatory = true)]
        public Guid Protocol;

        bool isDebugEnabled = false;
        protected override void BeginProcessing()
        {
            if (MyInvocation.BoundParameters.TryGetValue("Debug", out object value))
            {
                if (value is SwitchParameter switchValue)
                {
                    isDebugEnabled = switchValue.ToBool();
                }
            }
            else
            {
                if (GetVariableValue("DebugPreference") is ActionPreference debugPreference)
                {
                    isDebugEnabled = debugPreference != ActionPreference.SilentlyContinue;
                }
            }
        }

        protected override void ProcessRecord()
        {
            using (var inputStream = Console.OpenStandardInput())
            {
                using (var outputStream = Console.OpenStandardOutput())
                {

                    if (isDebugEnabled)
                    {
                        Type type = typeof(PowerShell);
                        var version = type.Assembly.GetName().Version;
                        var name = type.Name;

                        byte[] buffer = Encoding.UTF8.GetBytes($"{name} {version.Major}.{version.Minor}.{version.MajorRevision}"+Environment.NewLine);

                        outputStream.Write(buffer, 0, buffer.Length);
                    }

                    ServerReader reader = new ServerReader(inputStream, outputStream, Protocol, isDebugEnabled);

                    reader.Invoke();
                }
            }
        }
    }
}
