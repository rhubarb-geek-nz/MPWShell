// Copyright (c) 2026 Roger Brown.
// Licensed under the MIT License.

using RhubarbGeekNz.MPWShell.ToolServer;
using System;

namespace RhubarbGeekNz.ToolServer
{
	internal class Program
	{
		internal static void Main(string [] args)
		{
			Guid protocol = Guid.Parse(args[0]);

			using (var inputStream = Console.OpenStandardInput())
			{
				using (var outputStream = Console.OpenStandardOutput())
				{
					ServerReader reader = new ServerReader(inputStream, outputStream, protocol, false);

					reader.Invoke();
				}
			}
		}
	}
}
