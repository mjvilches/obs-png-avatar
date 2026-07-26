OBS PNG Avatar Plugin 🎙️✨
A native OBS Studio plugin that turns your microphone into an animated, audio-reactive PNG avatar.

Say goodbye to capturing external windows from Discord Streamkit or Veadotube! This plugin runs natively inside OBS, giving you a lightweight, zero-latency, all-in-one PNGTuber solution directly on your canvas.

✨ Features
3-State Audio Reactivity: Automatically switches between Default (Silent), Talking, and Shouting images based on your microphone volume.

Customizable Decibel Thresholds: Fine-tune exactly how loud you need to be to trigger the "Talking" and "Shouting" states.

Built-in Animations: Bring your avatar to life with GPU-accelerated animations (Bounce, Scale, Wobble).

Independent Animation States: Set different animations for different volume levels (e.g., a gentle bounce when talking, and a chaotic wobble when shouting!).

Zero Window Capture: Renders natively as an OBS Video Source, meaning no background apps to run, no green screens to key out, and virtually zero CPU overhead.

📥 Installation
(Note: Update this section once you publish your first Release)

Go to the Releases page and download the latest .zip or installer for your operating system.

Extract the contents directly into your main OBS Studio installation folder (usually C:\Program Files\obs-studio on Windows).

Restart OBS Studio.

🛠️ How to Use
Setting up your avatar takes less than a minute:

Add the Source: In your OBS Scene, click the + button under Sources and select PNG Avatar (Audio Reactive).

Link Your Mic: In the properties window, click the Microphone Source dropdown and select your active audio input (e.g., Mic/Aux).

Load Your Images: Browse and select your three character states:

Default Image: When you are silent.

Talking Image: When you are speaking normally.

Shouting Image: When you are loud/laughing.

Set Your Thresholds:

Speak normally into your mic and lower the Talking Threshold (dB) slider until your talking image appears reliably (usually around -40 dB to -30 dB).

Make a loud noise and adjust the Shouting Threshold (dB) so it only triggers when you are genuinely loud (usually around -15 dB to -5 dB).

Animate It! Scroll down to the Animation settings to add a Bounce, Scale, or Wobble effect. You can adjust the intensity of the movement and the global animation speed to fit your character's vibe.

💻 Building from Source
This plugin was built using the official obs-plugintemplate.

To compile this yourself:

Clone this repository.

Make sure you have CMake and your platform's build tools installed (Visual Studio on Windows, Xcode on macOS, GCC/Clang on Linux).

Generate the build files using CMake.

Compile the project. The resulting .dll, .so, or .dylib will be placed in your build directory, ready to be copied to your OBS plugins folder.
