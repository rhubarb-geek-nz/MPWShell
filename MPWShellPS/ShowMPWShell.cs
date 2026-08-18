// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using RhubarbGeekNz.MPWShell.ToolServer;
using System.Management.Automation;

namespace RhubarbGeekNz.MPWShell
{
    [Cmdlet(VerbsCommon.Show, "MPWShell")]
    sealed public class ShowMPWShell : PSCmdlet
    {
        [Parameter(Mandatory = true, ValueFromPipeline = true, HelpMessage = "String Message", Position = 0)]
        public string Message;


        protected override void ProcessRecord()
        {
            IMPWShell shell = (IMPWShell)GetVariableValue("MPW Shell");
            shell.Alert(Message);
        }
    }
}
