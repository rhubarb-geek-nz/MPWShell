# Copyright (c) 2026 Roger Brown.
# Licensed under the MIT License.

$ErrorActionPreference = 'Stop'

# MPW Shell starts the PowerShell tool server using these registry entries.
#
# ToolServer is used to launch the process
#
# InitialScript is used when each Runspace starts

# Check that the modules are installed

foreach ($name in 'ToolServer','MPWShell')
{
	$moduleName='rhubarb-geek-nz.'+$name
	Import-Module -Name $moduleName
	Get-Module -Name $moduleName
}

# Identify the path for PowerShell

$name = [Environment]::ProcessPath

if (-not $name)
{
	$name = (Get-Process -pid $PID).Path
}

$tool = "`"$name`" -NoProfile -NoLogo -NonInteractive -Command Invoke-MPWShell.ToolServer -Protocol 0c63fba6-7c2a-4b72-8de0-b3bd579dedaa"

$initial = "Set-Location `$HOME; if (`$PSStyle) { `$PSStyle.OutputRendering = 'PlainText' }; Import-Module rhubarb-geek-nz.MPWShell"

# ensure the key exists for the current user

$RegistryPath = "HKCU:\SOFTWARE\rhubarb-geek-nz\MPW Shell"

if (-not (Test-Path $RegistryPath))
{
	$null = New-Item -Path $RegistryPath -Force
}

# populate the rquired values

$properties = @{
	Name='ToolServer'
	Value=$tool
	PropertyType='String'},@{
	Name='InitialScript'
	Value=$initial
	PropertyType='String'},@{
	Name='TextLimit'
	Value=0x100000
	PropertyType='DWord'}

foreach ($property in $properties)
{
	try
	{
		$null = Get-ItemProperty -Path $RegistryPath -Name $property.Name
	}
	catch
	{
		$null = New-ItemProperty -Path $RegistryPath @property
	}
}

$properties | ForEach-Object { [pscustomobject]@{ Name = $_.Name ; Value = Get-ItemPropertyValue -Path $RegistryPath -Name $_.Name } } | Format-Table
