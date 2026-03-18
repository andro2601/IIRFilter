# IIRFilter
A high-performance, precision IIR high-pass and low-pass filter plugin built with the JUCE framework. This plugin offers 6 different analog filter approximations, utilizing the Bilinear Transform with exact frequency pre-warping.

To ensure mathematical accuracy and stability, the complex pole-zero prototypes for Elliptic and Bessel filters are pre-calculated using custom Python scripts (scipy.signal) and embedded directly into the C++ DSP code as Look-Up Tables (LUTs).
The chosen design method for this filter is the Bilinear Transform, allowing the user to choose between the following approximations:

## Butterworth
The "maximally flat" filter. It exhibits no ripples in the passband or the stopband, making it the most mathematically smooth general-purpose filter. The trade-off is a relatively gentle transition band (roll-off) compared to other designs of the same order.

## Chebyshev I
Achieves a significantly steeper roll-off than the Butterworth by allowing ripples in the passband. The stopband remains smooth (monotonic). It is ideal when a sharp frequency cut is required and minor gain fluctuations before the cutoff are acceptable.

## Chebyshev II
The inverse of Type I. It features a perfectly flat passband but allows ripples (bounces) in the stopband. It provides a steeper roll-off than Butterworth without coloring the frequencies you want to keep, at the cost of a hard limit on stopband attenuation.

## Elliptic (Cauer)
The most aggressive filter design. It allows ripples in both the passband and the stopband to achieve the absolute steepest possible transition between the two. This comes at the cost of severe non-linear phase distortion and time-domain ringing.

## Bessel 0Hz
The true zero-overshoot prototype. It scales the poles to ensure the group delay remains as flat as possible at DC (0Hz), meaning the time-domain pulse shape remains perfectly consistent across all filter orders. Custom Python-generated correction factors are applied in C++ to align the cutoff frequency accurately.

## Bessel -3dB
Unlike the filters above which prioritize frequency separation, the Bessel prioritizes the time domain. It features a maximally linear phase response, which preserves the shape of transient signals without smearing them. This variant is frequency-normalized so the -3dB point stays locked to the UI slider.

# Build
Prerequisites:
- JUCE Framework (v7.x or later)
- CMake (v3.20+) or Projucer
- A modern C++ compiler (MSVC, Clang, or GCC) supporting C++17

## HOWTO
Clone the JUCE framework (if you don't have it):
- git clone https://github.com/juce-framework/JUCE.git

Clone this repository
- git clone https://github.com/andro2601/IIRFilter.git

Open Projucer
- your\path\to\JUCE\extras\Projucer\Builds\VisualStudio2026\x64\Debug\App\Projucer.exe

In Projucer, open the IIRFilter.jucer file from this repository and choose the desired exporter (default: Visual Studio 2026).
From there, build the plugin in Release mode either as a Standalone application or as VST3/AAX.
