import os
import subprocess
import time

stdArrayUtilityUnitTestEnabled = False
streamingUnitCookerEnabled = False
vulkanTest01Enabled = True

#todo: os.chdir("E:\Code\VulkanTest01\VulkanTest01") -- zero %errorlevel% means success

#Path variable naming terminology:
#   Filename refers to a filename with no preceding path
#   RootedPath refers to a path that starts with a drive letter
#   FullPath refers to a path that ends in a filename
#   For consistency, always write RootedFullPath (or one or the other of the two path descriptors, as appropriate)

devenvRootedFullPath=r"C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\devenv.com"
baseRootedPath = r"E:\Code\VulkanTest01"
slnFilename=r"VulkanTest01.sln"

debug = "Debug"
profile = "Profile"
release = "Release"
shipping = "Shipping"

debugDirectory = debug
profileDirectory = profile
releaseDirectory = release
shippingDirectory = shipping

visualStudioConfigurationDebug=debug
visualStudioConfigurationsVulkanTest01=[debug, profile, release, shipping]
visualStudioConfigurationsStreamingUnitCooker=[debug, release]
platform_x86="x86"
platform_x64="x64"
platforms=[platform_x64,platform_x86]

def x64Directory(subdirectory):
    return "x64\%s" % subdirectory

buildDirectoriesVulkanTest01 = [profileDirectory, releaseDirectory, shippingDirectory, x64Directory(debugDirectory), x64Directory(profileDirectory), x64Directory(releaseDirectory), x64Directory(shippingDirectory)]
buildDirectoriesUnitCooker = [debugDirectory, releaseDirectory, x64Directory(debugDirectory), x64Directory(releaseDirectory)]



def Print(s):
    print("BUILD_SYSTEM: %s" % s)

def Fail(s):
    Print("FAILED: %s" % s)
    quit(1)

def OsSystem(cmd):
    Print(cmd)
    r = os.system(cmd)
    if r != 0:
        Fail("Above command failed returned %s" % r)
    return r
    
def SubProcess(command, exitOnFail=True):
    Print("%s" % command)
    try:
        retVal = subprocess.check_output(command, shell=True)#according to the python documentation subprocess.run waits for the process to end
        return retVal
    except subprocess.CalledProcessError as e:
        Print("ERROR SubProcess() returned %s" % e.output.decode())
        if exitOnFail:
            Fail("above error is fatal")
        return e.output
    
def FunctionCallConditional(enabled, function, stringIdentifier):
    if enabled:
        function()
    else:
        Print("%s DISABLED" % stringIdentifier)
                
def VisualStudioBuild(configuration, platform, vcxprojFullPath):
    SubProcess(r'"%s" "%s\%s" /Build "%s|%s" /Project "%s\%s"' % (devenvRootedFullPath, baseRootedPath, slnFilename, configuration, platform, baseRootedPath, vcxprojFullPath))

def VisualStudioBuildSet(vcxprojFullPath, visualStudioConfigurations, exclusions={platform_x86:[visualStudioConfigurationDebug]}):#unknown library problem with Debug|x86 that I don't care to solve)
    for platform in platforms:
        for visualStudioConfiguration in visualStudioConfigurations:
            if not (exclusions and platform in exclusions and visualStudioConfiguration in exclusions[platform]):
                VisualStudioBuild(visualStudioConfiguration, platform, vcxprojFullPath)

def StdArrayUtilityUnitTest_Build():
    VisualStudioBuildSet(r"stdArrayUtilityUnitTest\stdArrayUtilityUnitTest.vcxproj", visualStudioConfigurationsVulkanTest01)
FunctionCallConditional(stdArrayUtilityUnitTestEnabled, StdArrayUtilityUnitTest_Build, "StdArrayUtilityUnitTest_Build")

def StreamingUnitCooker_Build():
    VisualStudioBuildSet(r"StreamingUnitCooker\StreamingUnitCooker.vcxproj", visualStudioConfigurationsStreamingUnitCooker)
FunctionCallConditional(streamingUnitCookerEnabled, StreamingUnitCooker_Build, "StreamingUnitCooker_Build")

def VulkanTest01_Build():
    VisualStudioBuildSet(r"VulkanTest01\VulkanTest01.vcxproj", visualStudioConfigurationsVulkanTest01)
FunctionCallConditional(vulkanTest01Enabled, VulkanTest01_Build, "VulkanTest01_Build")


def ChdirWorkingPath(workingDirectoryPath):
    dir = "%s\%s" % (baseRootedPath, workingDirectoryPath)
    Print("chdir %s" % dir)
    return os.chdir(dir)

def ExecutableRootedFullPath(executableFullPath):
    return "%s\%s" % (baseRootedPath, executableFullPath)

def WindowsExeExtensionAppend(executableFilename):
    return "%s.exe" % executableFilename

def Run(workingDirectoryPath, executableFullPath):
    ChdirWorkingPath(workingDirectoryPath)
    OsSystem(ExecutableRootedFullPath(executableFullPath))

def StdArrayUtilityUnitTest_Run():
    stdArrayUtilityUnitTestString = "stdArrayUtilityUnitTest"
    for buildDirectory in buildDirectoriesVulkanTest01:
        Run(stdArrayUtilityUnitTestString, "%s\%s" % (buildDirectory, WindowsExeExtensionAppend(stdArrayUtilityUnitTestString)))
FunctionCallConditional(stdArrayUtilityUnitTestEnabled, StdArrayUtilityUnitTest_Run, "StdArrayUtilityUnitTest_Run")
    
def StreamingUnitCooker_Run():
    streamingUnitCookerString = "StreamingUnitCooker"
    for buildDirectory in buildDirectoriesUnitCooker:
        Run(streamingUnitCookerString, "%s\%s" % (buildDirectory, WindowsExeExtensionAppend(streamingUnitCookerString)))
FunctionCallConditional(streamingUnitCookerEnabled, StreamingUnitCooker_Run, "StreamingUnitCooker_Run")

def VulkanTest01_Run():
    for buildDirectory in buildDirectoriesVulkanTest01:
        ChdirWorkingPath("VulkanTest01")
        executableRootedFullPath = ExecutableRootedFullPath("%s\%s" % (buildDirectory, WindowsExeExtensionAppend("vulkanTest01")))
       
        Print(executableRootedFullPath)
        subprocess.Popen(executableRootedFullPath)
        
        secondsToWait = 10
        Print("Waiting for %s seconds" % secondsToWait)
        time.sleep(secondsToWait)
        OsSystem("taskkill /F /IM VulkanTest01.exe")
FunctionCallConditional(vulkanTest01Enabled, VulkanTest01_Run, "VulkanTest01_Run")

Print("SUCCESS!")