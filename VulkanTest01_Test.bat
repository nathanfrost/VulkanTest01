REM Suppress "press a key to continue" behavior on process exit to prevent processes from exiting cleanly during unit test
SET CL=/DNTF_NO_KEYSTROKE_TO_END_PROCESS=1

python VulkanTest01_Test.py
pause