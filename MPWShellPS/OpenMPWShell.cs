// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using RhubarbGeekNz.MPWShell.ToolServer;
using System;
using System.Management.Automation;

namespace RhubarbGeekNz.MPWShell
{
    [Cmdlet(VerbsCommon.Open, "MPWShell")]
    [Alias("open")]
    sealed public class OpenMPWShell : PSCmdlet
    {
        [Parameter(Mandatory = true, ParameterSetName = "path", Position = 0)]
        public string[] Path;

        [Parameter(Mandatory = true, ParameterSetName = "literal", ValueFromPipeline = true)]
        public string[] LiteralPath;

        protected override void ProcessRecord()
        {
            IMPWShell shell = (IMPWShell)GetVariableValue("MPW Shell");

            if (Path != null)
            {
                foreach (string path in Path)
                {
                    var paths = GetResolvedProviderPathFromPSPath(path, out var providerPath);

                    if ("FileSystem".Equals(providerPath.Name))
                    {
                        foreach (string item in paths)
                        {
                            shell.Open(item);
                        }
                    }
                    else
                    {
                        WriteError(new ErrorRecord(new Exception($"Provider {providerPath.Name} not handled"), "ProviderError", ErrorCategory.NotImplemented, providerPath));
                    }
                }
            }

            if (LiteralPath != null)
            {
                foreach (string literalPath in LiteralPath)
                {
                    string item = GetUnresolvedProviderPathFromPSPath(literalPath);
                    shell.Open(item);
                }
            }
        }
    }
}
