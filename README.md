# IIR Low Pass and High Pass filters
This repository contains the JUCE code for an implementation of various low-pass and high-pass filter approximations for a variable filter order, e.g. number of biquads.
The chosen design method for this filter is the Bilinear Transform, allowing the user to choose between the following approximations:
## Butterworth
## Chebyshev I
## Chebyshev II
## Elliptic
## Bessel
Two different variations of the Bessel approximations are available:
### Bessel 0Hz
The 0Hz alludes to the amplitude response being maximally flat
