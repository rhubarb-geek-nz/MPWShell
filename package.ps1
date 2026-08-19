param(
	$CertificateThumbprint = '601A8B683F791E51F647D34AD102C38DA4DDB65F',
	$BundleThumbprint = '5F88DFB53180070771D4507244B2C9C622D741F8'
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

trap
{
	throw $PSItem
}

[int]$VersionNumber = $null | git log --oneline . | Measure-Object | ForEach-Object { $_.Count }
$VersionNumberHex = $VersionNumber.ToString('X8')
$list = @(
	( [int32]::Parse($VersionNumberHex.Substring(0,4),[System.Globalization.NumberStyles]::HexNumber) + 0),
	( [int32]::Parse($VersionNumberHex.Substring(4,2),[System.Globalization.NumberStyles]::HexNumber) + 9),
	( [int32]::Parse($VersionNumberHex.Substring(6,2),[System.Globalization.NumberStyles]::HexNumber) + 0)
)

$Version = "$($list[0]).$($list[1]).$($list[2])"

Write-Output "Version is $Version from $VersionNumber"

foreach ($project in 'ToolServerPS', 'MPWShellPS')
{
	Push-Location -LiteralPath $project

	try
	{
		foreach ($dir in 'bin','obj')
		{
			if (Test-Path -LiteralPath $dir -PathType Container)
			{
				Remove-Item $dir -Recurse
			}
		}

		dotnet publish --configuration Release -p:CertificateThumbprint=$CertificateThumbprint -p:Version=$Version

		if ($LastExitCode)
		{
			throw $LastExitCode
		}
	}
	finally
	{
		Pop-Location
	}
}

if ($IsWindows -or ( 'Desktop' -eq $PSEdition ))
{
	Push-Location -LiteralPath 'Win32'

	try
	{
		$Win32Dir = $PWD

		foreach ($DIR in 'obj', 'bin', 'bundle')
		{
			if (Test-Path -LiteralPath $DIR)
			{
				Remove-Item -LiteralPath $DIR -Force -Recurse
			}
		}

		Push-Location -LiteralPath 'HtmlHelp'

		try
		{
			$output = 'mpwshell.chm'

			if (Test-Path -LiteralPath $output -PathType Leaf)
			{
				Remove-Item -LiteralPath $output
			}

			$dir = Get-ChildItem env: | ForEach-Object { if ($_.Name -eq 'ProgramFiles(x86)') { $_.Value } }

			$app = "$dir\HTML Help Workshop\hhc.exe"

			Get-Item -LiteralPath $app

			& $app mpwshell.hhp

			Write-Output "`$LastExitCode = $LastExitCode"

			Get-Item -LiteralPath $output
		}
		finally
		{
			Pop-Location
		}

		foreach ($EDITION in 'Community', 'Professional')
		{
			$VCVARSDIR = "${Env:ProgramFiles}\Microsoft Visual Studio\18\$EDITION\VC\Auxiliary\Build"

			if ( Test-Path -LiteralPath $VCVARSDIR -PathType Container )
			{
				break
			}
		}

		$VCVARSARM = 'vcvarsarm.bat'
		$VCVARSARM64 = 'vcvarsarm64.bat'
		$VCVARSAMD64 = 'vcvars64.bat'
		$VCVARSX86 = 'vcvars32.bat'
		$VCVARSHOST = 'vcvars32.bat'

		switch ($Env:PROCESSOR_ARCHITECTURE)
		{
			'AMD64' {
				$VCVARSX86 = 'vcvarsamd64_x86.bat'
				$VCVARSARM = 'vcvarsamd64_arm.bat'
				$VCVARSARM64 = 'vcvarsamd64_arm64.bat'
				$VCVARSHOST = $VCVARSAMD64
			}
			'ARM64' {
				$VCVARSX86 = 'vcvarsarm64_x86.bat'
				$VCVARSARM = 'vcvarsarm64_arm.bat'
				$VCVARSAMD64 = 'vcvarsarm64_amd64.bat'
				$VCVARSHOST = $VCVARSARM64
			}
			'X86' {
				$VCVARSXARM64 = 'vcvarsx86_arm64.bat'
				$VCVARSARM = 'vcvarsx86_arm.bat'
				$VCVARSAMD64 = 'vcvarsx86_amd64.bat'
			}
			Default {
				throw "Unknown architecture $Env:PROCESSOR_ARCHITECTURE"
			}
		}

		$VCVARSARCH = @{'arm' = $VCVARSARM; 'arm64' = $VCVARSARM64; 'x86' = $VCVARSX86; 'x64' = $VCVARSAMD64}

		$ARCHLIST = ( $VCVARSARCH.Keys | ForEach-Object {
			$VCVARS = $VCVARSARCH[$_];
			if ( Test-Path -LiteralPath "$VCVARSDIR/$VCVARS" -PathType Leaf )
			{
				$_
			}
		} | Sort-Object )

		$ARCHLIST | ForEach-Object {
			New-Object PSObject -Property @{
				Architecture=$_;
				Environment=$VCVARSARCH[$_]
			}
		} | Format-Table -Property Architecture,'Environment'

		$ARCHLIST | ForEach-Object {
			$ARCH = $_

			$VCVARS = ( '{0}\{1}' -f $VCVARSDIR, $VCVARSARCH[$ARCH] )

			$VersionStr4 = "$Version.0"
			$VersionInt4 = $VersionStr4.Replace(".",",")
			$VersionInt2 = "$($list[0]).$($list[1])"

			switch ($ARCH)
			{
				'x86'   { $MSIUPGRADECODE='14EE0031-B0AA-43FF-B459-6A41114B1A44' ; $MSISHORTCUTID='AA162DA4-A409-4757-A52B-D6AEDF0C2455' ; $MSIIS64BIT='no' ; $MSIINSTALLVERS='200' ; $MSIPROGFILES='ProgramFilesFolder' }
				'x64'   { $MSIUPGRADECODE='F22BFA67-F0A9-4F84-9984-460AC3899DDC' ; $MSISHORTCUTID='878442A9-6C08-4154-BCC8-4F59369A012A' ; $MSIIS64BIT='yes' ; $MSIINSTALLVERS='200' ; $MSIPROGFILES='ProgramFiles64Folder' }
				'arm'   { $MSIUPGRADECODE='1D84D748-D116-4111-B6EB-42735335C69F' ; $MSISHORTCUTID='CF192E12-D0B9-4620-B8F0-63A717216B0B' ; $MSIIS64BIT='no' ; $MSIINSTALLVERS='500' ; $MSIPROGFILES='ProgramFilesFolder' }
				'arm64' { $MSIUPGRADECODE='015FB924-8B51-4F4C-9FB2-57BBF11166AF' ; $MSISHORTCUTID='D55E8D7C-DDD6-4372-8746-3B3F0DA91BF0' ; $MSIIS64BIT='yes' ; $MSIINSTALLVERS='500' ; $MSIPROGFILES='ProgramFiles64Folder' }
				default { throw "unknown $ARCH" }
			}

			$xmlDoc = [System.Xml.XmlDocument](Get-Content "Package.appxmanifest")

			$nsMgr = New-Object -TypeName System.Xml.XmlNamespaceManager -ArgumentList $xmlDoc.NameTable

			$nsmgr.AddNamespace("man", "http://schemas.microsoft.com/appx/manifest/foundation/windows10")

			$xmlNode = $xmlDoc.SelectSingleNode("/man:Package/man:Identity", $nsmgr)

			$xmlNode.ProcessorArchitecture = "$ARCH"
			$xmlNode.Version = $VersionStr4

			$xmlDoc.Save("$Win32Dir\AppxManifest.xml")

			@"
CALL "$VCVARS"
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
NMAKE /NOLOGO clean mpwshell_STR3="$Version"
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
NMAKE /NOLOGO mpwshell_INT2="$VersionInt2" mpwshell_STR4="$VersionStr4" mpwshell_INT4="$VersionInt4" CertificateThumbprint="$CertificateThumbprint" BundleThumbprint="$BundleThumbprint" MSIUPGRADECODE="$MSIUPGRADECODE" MSISHORTCUTID="$MSISHORTCUTID" MSIPROGFILES="$MSIPROGFILES" MSIIS64BIT="$MSIIS64BIT" MSIINSTALLVERS="$MSIINSTALLVERS" mpwshell_STR3="$Version" BundleThumbprint="$BundleThumbprint"
EXIT %ERRORLEVEL%
"@ | & "$env:COMSPEC"

			if ($LastExitCode -ne 0)
			{
				exit $LastExitCode
			}
		}

		@"
CALL "$VCVARSDIR\$VCVARSHOST"
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
NMAKE /NOLOGO mpwshell_STR3="$Version" mpwshell_STR4="$VersionStr4" mpwshell_INT4="$VersionInt4" CertificateThumbprint="$CertificateThumbprint" BundleThumbprint="$BundleThumbprint" "MPWShell-$Version.msixbundle"
EXIT %ERRORLEVEL%
"@ | & "$env:COMSPEC"

		if ($LastExitCode -ne 0)
		{
			exit $LastExitCode
		}

		Invoke-Command -ScriptBlock {
			'../ToolServerPS/bin/Release/netstandard2.0/publish/RhubarbGeekNz.ToolServer.dll', `
			'../MPWShellPS/bin/Release/netstandard2.0/publish/RhubarbGeekNz.MPWShell.dll' | Get-Item
			Get-ChildItem -LiteralPath 'bin' -Recurse -Filter 'MPWShell.exe'
		} | ForEach-Object { $_.VersionInfo } 

		Get-ChildItem -LiteralPath . -Filter "MPWShell-$Version-*.msi" | Get-AuthenticodeSignature | Format-Table
	}
	finally
	{
		Pop-Location
	}
}
