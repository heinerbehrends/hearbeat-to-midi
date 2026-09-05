import time
Import("env")

# Force PlatformIO to sleep for 2 seconds right after flashing completes
if "test" in env.GetBuildType():
    env.AddPostAction("upload", lambda *_, **__: time.sleep(2))
