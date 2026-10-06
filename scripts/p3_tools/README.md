# P3 Audio Conversion and Playback Tools

This directory contains Python scripts for working with P3 audio files:

## 1. Audio conversion tool (convert_audio_to_p3.py)

Converts standard audio files to P3 format (a streaming structure with a 4-byte header and Opus packets) and normalizes loudness.

### Usage

```bash
python convert_audio_to_p3.py <input audio file> <output P3 file> [-l LUFS] [-d]
```

The optional argument `-l` specifies the target loudness for normalization; the default is -16 LUFS. The optional argument `-d` disables loudness normalization.

If the input audio meets any of the following conditions, use  `-d`  to disable loudness normalization:
- The audio is very short
- The audio loudness has already been adjusted
- The audio comes from the default TTS (Xiaozhi’s current TTS already defaults to -16 LUFS)

Example:
```bash
python convert_audio_to_p3.py input.mp3 output.p3
```

## 2. P3 audio playback tool (play_p3.py)

Plays P3 audio files.

### Features

- Decodes and plays P3 audio files
- Applies a fade-out at the end of playback or when interrupted, preventing audio artifacts
- Accepts the file to play as a command-line argument

### Usage

```bash
python play_p3.py <Path to the P3 file>
```

Example:
```bash
python play_p3.py output.p3
```

## 3. Audio conversion back tool (convert_p3_to_audio.py)

Converts P3 files back to standard audio formats.

### Usage

```bash
python convert_p3_to_audio.py <Input P3 file> <Output audio file>
```

The output audio file must have an extension.

Example:
```bash
python convert_p3_to_audio.py input.p3 output.wav
```
## 4. Batch Audio/P3 Conversion Tool

A graphical tool for batch conversion from audio to P3 and from P3 to audio

![](./img/img.png)

### Usage：
```bash
python batch_convert_gui.py
```

## Installing Dependencies

Before using these scripts, install the required Python packages:

```bash
pip install librosa opuslib numpy tqdm sounddevice pyloudnorm soundfile
```

Alternatively, install from the provided requirements.txt:

```bash
pip install -r requirements.txt
```

## P3 Format

P3 is a simple streaming audio format with the following structure:
- Each audio frame consists of a 4-byte header and an Opus-encoded packet
- Header format: [1-byte type, 1-byte reserved, 2-byte length]
- The sample rate is fixed at 16000 Hz, mono
- Each frame is 60 ms long
