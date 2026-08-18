// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

namespace RhubarbGeekNz.MPWShell.ToolServer
{
    public interface IMPWShell
    {
        void New(string content = null);
        void Open(string filePath);
        void Alert(string message);
    }
}
