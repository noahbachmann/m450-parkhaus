# Links Winsock for the PC simulator on Windows; other systems need nothing extra.
import platform

Import("env")

if platform.system() == "Windows":
    env.Append(LIBS=["ws2_32"])
