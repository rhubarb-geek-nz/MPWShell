// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using RhubarbGeekNz.MPWShell.ToolServer;
using System.Management.Automation;

namespace RhubarbGeekNz.MPWShell
{
    [Cmdlet(VerbsCommon.New, "MPWShell")]
    [Alias("new")]
    sealed public class NewMPWShell : PSCmdlet
    {
        [Parameter(ValueFromPipeline = true, HelpMessage = "String Content Data", Position = 0)]
        [AllowNull()]
        [AllowEmptyCollection()]
        public string Content;
        
        protected override void ProcessRecord()
        {
            IMPWShell shell = (IMPWShell)GetVariableValue("MPW Shell");
            shell.New(Content);
        }
    }
}
