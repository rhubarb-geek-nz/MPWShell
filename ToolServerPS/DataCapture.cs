// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using System;
using System.IO;
using System.Management.Automation;
using System.Text;

namespace RhubarbGeekNz.MPWShell.ToolServer
{
    sealed internal class DataCapture
    {
        readonly bool isDebugEnabled;
        readonly Stream outputStream;
        readonly object mutex;
        internal readonly int runtimeId;
        internal readonly int clientId;
        
        internal DataCapture(Stream outputStream, object mutex, int clientId, int runtimeId, bool isDebugEnabled)
        {
            this.outputStream = outputStream;
            this.mutex = mutex;
            this.clientId = clientId;
            this.runtimeId = runtimeId;
            this.isDebugEnabled = isDebugEnabled;
        }

        internal DataCapture Clone(int runtimeId)
        {
            return new DataCapture(outputStream, mutex, clientId, runtimeId, isDebugEnabled);
        }

        internal void WriteString(int type, string str)
        {
            lock (mutex)
            {
                byte[] data = Encoding.UTF8.GetBytes(str);

                byte[][] header = new[]
                {
                    BitConverter.GetBytes(type),
                    BitConverter.GetBytes(runtimeId),
                    BitConverter.GetBytes(clientId),
                    BitConverter.GetBytes(data.Length),
                    data
                };

                foreach (var packet in header)
                {
                    outputStream.Write(packet,0, packet.Length);
                }

                outputStream.Flush();
            }
        }

        private void WriteString(string str)
        {
            WriteString(0x40, str);
        }

        internal void DataAdded(object sender, DataAddedEventArgs e)
        {
            if (sender is PSDataCollection<PSObject> output)
            {
                PSObject element = output[e.Index];

                if (element != null)
                {
                    object obj = element.BaseObject;

                    if (obj is String str)
                    {
                        WriteString(str);

                    }
                    else
                    {
                        WriteString(obj.ToString());
                    }
                }
            }
        }

        internal void WarningAdded<T>(object sender, DataAddedEventArgs e)
        {
            if (sender is PSDataCollection<T> output)
            {
                T element = output[e.Index];

                if (element != null)
                {
                    WriteString("WARNING: " + element.ToString());
                }
            }
        }

        internal void InformationAdded<T>(object sender, DataAddedEventArgs e)
        {
            if (sender is PSDataCollection<T> output)
            {
                T element = output[e.Index];

                if (element != null)
                {
                    WriteString(element.ToString());
                }
            }
        }

        internal void VerboseAdded<T>(object sender, DataAddedEventArgs e)
        {
            if (sender is PSDataCollection<T> output)
            {
                T element = output[e.Index];

                if (element != null)
                {
                    WriteString("VERBOSE: " + element.ToString());
                }
            }
        }

        internal void DebugAdded<T>(object sender, DataAddedEventArgs e)
        {
            if (sender is PSDataCollection<T> output)
            {
                T element = output[e.Index];

                if (element != null)
                {
                    WriteString("DEBUG: " + element.ToString());
                }
            }
        }

        internal void WriteError(ErrorRecord record)
        {
            string prefix = null;

            if (record.CategoryInfo != null)
            {
                prefix = record.CategoryInfo.Activity;
            }

            if (string.IsNullOrEmpty(prefix) && record.CategoryInfo != null)
            {
                prefix = record.CategoryInfo.TargetName;
            }

            if (string.IsNullOrEmpty(prefix))
            {
                WriteString(record.ToString());
            }
            else
            {
                WriteString(prefix + ": " + record.ToString());
            }
        }

        internal void ErrorAdded(object sender, DataAddedEventArgs e)
        {
            if (sender is PSDataCollection<ErrorRecord> output)
            {
                ErrorRecord element = output[e.Index];

                WriteError(element);
            }
        }
    }
}
