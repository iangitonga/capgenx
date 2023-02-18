import os
import subprocess


BASE_PATH = os.path.abspath(os.getcwd())


# Unzip application models. -d temp_for_zip_extract
# TODO: Should be done by application itself.

models_path = os.path.join(BASE_PATH, "bin", "assets", "models")
tiny_model_path = os.path.join(models_path, "tiny.zip")
target_model_path = os.path.join(models_path, "tiny")
subprocess.run([f"unzip {tiny_model_path} -d {target_model_path}"], shell=True, check=True)


# Libtorch download binaries.
LIBTORCH_CPU_DOWNLOAD_URL = "https://download.pytorch.org/libtorch/nightly/cpu/libtorch-shared-with-deps-latest.zip"
LIBTORCH_DOWNLOAD_PATH = os.path.join(BASE_PATH, "third_party")
os.chdir(LIBTORCH_DOWNLOAD_PATH)

subprocess.run([f"wget {LIBTORCH_CPU_DOWNLOAD_URL}"], shell=True, check=True)
subprocess.run([f"unzip libtorch-shared-with-deps-latest.zip"], shell=True, check=True)


# FFmpeg Build.
FFMPEG_PATH = os.path.join(BASE_PATH, "third_party", "ffmpeg")
FFMPEG_PREFIX_PATH = os.path.join(FFMPEG_PATH, 'build_capgen')

os.chdir(FFMPEG_PATH)

FFMPEG_CONFIG = [
	"./configure",
	f"--prefix={FFMPEG_PREFIX_PATH}",
	"--disable-programs",
	"--disable-ffmpeg",
	"--disable-ffplay",
	"--disable-ffprobe",
	"--disable-doc",
	"--disable-htmlpages",
	"--disable-manpages",
	"--disable-podpages",
	"--disable-txtpages",
]

FFMPEG_CONFIG_ARGS = [" ".join(FFMPEG_CONFIG)]
subprocess.run(FFMPEG_CONFIG_ARGS, shell=True, check=True)
subprocess.run(["make"], shell=True, check=True)
subprocess.run(["make install"], shell=True, check=True)

