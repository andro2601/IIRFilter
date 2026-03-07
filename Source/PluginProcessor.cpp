/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dsp;

//==============================================================================
IIRFilterAudioProcessor::IIRFilterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // startup (call before any logging)
    /*Logger::setCurrentLogger(new FileLogger(File::getSpecialLocation(File::userDocumentsDirectory)
        .getChildFile("IIRFilter.log")));*/
}

IIRFilterAudioProcessor::~IIRFilterAudioProcessor()
{
    /*Logger::setCurrentLogger(nullptr); // deletes the FileLogger*/
}

//==============================================================================
const String IIRFilterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool IIRFilterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool IIRFilterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool IIRFilterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double IIRFilterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int IIRFilterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int IIRFilterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void IIRFilterAudioProcessor::setCurrentProgram (int index)
{
}

const String IIRFilterAudioProcessor::getProgramName (int index)
{
    return {};
}

void IIRFilterAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void IIRFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize oversampler but don't worry about constant latency yet
    oversampler = std::make_unique<Oversampling<double>>(2, 2, Oversampling<double>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing(samplesPerBlock);

    // Resize the workbench buffer (no audio processing here, just memory allocation)
    doubleBuffer.setSize(getMainBusNumOutputChannels(), samplesPerBlock);
    doubleBuffer.clear(); // Ensure it starts at zero!

    ProcessSpec spec;
    spec.sampleRate = sampleRate * 4.0;
    spec.maximumBlockSize = samplesPerBlock * 4;
    spec.numChannels = getMainBusNumOutputChannels();

    highPassChain.prepare(spec);
    highPassChain.reset();
    lowPassChain.prepare(spec);
    lowPassChain.reset();

    smoothedLpCutoff.reset(sampleRate, 0.001); // 1ms glide
	smoothedHpCutoff.reset(sampleRate, 0.001); // 1ms glide

    updateCoefficients(sampleRate);
}

void IIRFilterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool IIRFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void IIRFilterAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
		doubleBuffer.clear(i, 0, doubleBuffer.getNumSamples());
    }

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    updateCoefficients(getSampleRate());

    // Upsample to 64-bit (Float -> Double)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* floatRead = buffer.getReadPointer(ch);
        auto* doubleWrite = doubleBuffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            doubleWrite[i] = static_cast<double>(floatRead[i]);
        }
    }
    
    // Check if the buffer is silent
    if (buffer.getMagnitude(0, numSamples) < 0.000001f) // Roughly -120dB
    {
        // If the buffer is silent, we might still be 'ringing'
        if (++silentBlockCount > 100)
        {
            return; // Skip the filter math
        }
    }
    else
    {
        silentBlockCount = 0;
    }

    AudioBlock<double> doubleBlock(doubleBuffer.getArrayOfWritePointers(),
        doubleBuffer.getNumChannels(),
        numSamples);

    auto hpIsBypassed = parameters.getRawParameterValue("bypassHp")->load();
    auto lpIsBypassed = parameters.getRawParameterValue("bypassLp")->load();

    int approxType = static_cast<int>(parameters.getRawParameterValue("approximation")->load());
    if (approxType == 4) // "Bessel 0Hz"
    {
        auto oversampledBlock = oversampler->processSamplesUp(doubleBlock);

        if (!hpIsBypassed) {
            highPassChain.process(ProcessContextReplacing<double>(oversampledBlock));
        }
        if (!lpIsBypassed) {
            lowPassChain.process(ProcessContextReplacing<double>(oversampledBlock));
        }

        oversampler->processSamplesDown(doubleBlock);
    }
    else
    {
        // STANDARD: Just process at 44.1k / 48k
        auto context = ProcessContextReplacing<double>(doubleBlock);
        if (!hpIsBypassed) {
            highPassChain.process(context);
        }
        if (!lpIsBypassed) {
            lowPassChain.process(context);
        }	
    }
    
    // Cast back to 32-bit (Double -> Float) for the DAW
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* doubleRead = doubleBuffer.getReadPointer(ch);
        auto* floatWrite = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            floatWrite[i] = static_cast<float>(doubleRead[i]);
        }
    }
}

void IIRFilterAudioProcessor::updateCoefficients(double sampleRate) {
	float lpCutoffParam = parameters.getRawParameterValue("lpCutoff")->load();
	float hpCutoffParam = parameters.getRawParameterValue("hpCutoff")->load();
    float lpCutoff = smoothedLpCutoff.getNextValue();
    float hpCutoff = smoothedHpCutoff.getNextValue();
	int filterOrder = static_cast<int>(parameters.getRawParameterValue("filterOrder")->load());
	int approximationType = static_cast<int>(parameters.getRawParameterValue("approximation")->load());

    if (lpCutoffParam == lastLpCutoffParam && hpCutoffParam == lastHpCutoffParam && lpCutoff == lastLpCutoff && hpCutoff == lastHpCutoff && filterOrder == lastFilterOrder && approximationType == lastApproximationType) return;
    
    smoothedLpCutoff.setTargetValue(lpCutoffParam);
    smoothedHpCutoff.setTargetValue(hpCutoffParam);
	lastLpCutoffParam = lpCutoffParam;
	lastHpCutoffParam = hpCutoffParam;
    lastLpCutoff = lpCutoff;
    lastHpCutoff = hpCutoff;
	lastFilterOrder = filterOrder;
	lastApproximationType = approximationType;

    std::vector<IIR::Coefficients<double>> lpCoeffs;
    std::vector<IIR::Coefficients<double>> hpCoeffs;

	double lpCutoffDouble = static_cast<double>(lpCutoff);
	double hpCutoffDouble = static_cast<double>(hpCutoff);

    switch (approximationType) {
        case 0: // Butterworth
			butterworthCoefficients(lpCoeffs, hpCoeffs, lpCutoffDouble, hpCutoffDouble, filterOrder, sampleRate);
            break;
        case 1: // Chebyshev Type I
			chebyshevICoefficients(lpCoeffs, hpCoeffs, lpCutoffDouble, hpCutoffDouble, filterOrder, sampleRate);
            break;
        case 2: // Chebyshev Type II
			chebyshevIICoefficients(lpCoeffs, hpCoeffs, lpCutoffDouble, hpCutoffDouble, filterOrder, sampleRate);
            break;
        case 3: // Elliptic
			ellipticCoefficients(lpCoeffs, hpCoeffs, lpCutoffDouble, hpCutoffDouble, filterOrder, sampleRate);
            break;
		case 4: // Bessel 0Hz
			besselCoefficients(lpCoeffs, hpCoeffs, lpCutoffDouble, hpCutoffDouble, filterOrder, sampleRate);
            break;
        case 5: // Bessel -3dB
			besselCoefficients(lpCoeffs, hpCoeffs, lpCutoffDouble, hpCutoffDouble, filterOrder, sampleRate);
            break;
        default:
			jassertfalse; // Invalid approximation type index
    }

    if (filterOrder >= 2) {
        *lowPassChain.get<0>().state = lpCoeffs.at(0);
        *highPassChain.get<0>().state = hpCoeffs.at(0);

        lowPassChain.setBypassed<0>(false);
        lowPassChain.setBypassed<1>(true);
		lowPassChain.setBypassed<2>(true);
        lowPassChain.setBypassed<3>(true);
        highPassChain.setBypassed<0>(false);
        highPassChain.setBypassed<1>(true);
        highPassChain.setBypassed<2>(true);
        highPassChain.setBypassed<3>(true);
    }

    if (filterOrder >= 4) {
		*lowPassChain.get<1>().state = lpCoeffs.at(1);
		*highPassChain.get<1>().state = hpCoeffs.at(1);

        lowPassChain.setBypassed<1>(false);
        highPassChain.setBypassed<1>(false);
    }

    if (filterOrder >= 6) {
		*lowPassChain.get<2>().state = lpCoeffs.at(2);
		*highPassChain.get<2>().state = hpCoeffs.at(2);

		lowPassChain.setBypassed<2>(false);
		highPassChain.setBypassed<2>(false);
    }

    if (filterOrder >= 8) {
        *lowPassChain.get<3>().state = lpCoeffs.at(3);
        *highPassChain.get<3>().state = hpCoeffs.at(3);

        lowPassChain.setBypassed<3>(false);
        highPassChain.setBypassed<3>(false);
    }
}

void IIRFilterAudioProcessor::butterworthCoefficients(std::vector<IIR::Coefficients<double>>& lpCoeffs, std::vector<IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate) {
    // Pre-calculate intermediate variables
    double LPomega = 2.0 * MathConstants<double>::pi * lpCutoff / sampleRate;
    double LPsin = std::sin(LPomega);
    double LPcos = std::cos(LPomega);

    double HPomega = 2.0 * MathConstants<double>::pi * hpCutoff / sampleRate;
    double HPsin = std::sin(HPomega);
    double HPcos = std::cos(HPomega);

    // Calculate raw coefficients
    double LPb0 = (1.0 - LPcos) / 2.0;
    double LPb1 = 1.0 - LPcos;
    double LPb2 = (1.0 - LPcos) / 2.0;
    double LPa1 = -2.0 * LPcos;

    double HPb0 = (1.0 + HPcos) / 2.0;
    double HPb1 = -(1.0 + HPcos);
    double HPb2 = (1.0 + HPcos) / 2.0;
    double HPa1 = -2.0 * HPcos;

    int N = filterOrder;
    for (int i = 0; i < filterOrder / 2; ++i) {
        double Q = 1.0 / (2.0 * std::sin((2 * (i + 1) - 1) * MathConstants<double>::pi / (2 * N)));

        double LPalpha = LPsin / (2.0 * Q);
        double LPa0 = 1.0 + LPalpha;
        double LPa2 = 1.0 - LPalpha;

        double HPalpha = HPsin / (2.0 * Q);
        double HPa0 = 1.0 + HPalpha;
        double HPa2 = 1.0 - HPalpha;

        lpCoeffs.emplace_back(
            (LPb0 / LPa0),
            (LPb1 / LPa0),
            (LPb2 / LPa0),
            1.0, // a0 becomes 1
            (LPa1 / LPa0),
            (LPa2 / LPa0)
        );

        hpCoeffs.emplace_back(
            (HPb0 / HPa0),
            (HPb1 / HPa0),
            (HPb2 / HPa0),
            1.0, // a0 becomes 1
            (HPa1 / HPa0),
            (HPa2 / HPa0)
        );
    }
}

void IIRFilterAudioProcessor::chebyshevICoefficients(std::vector<IIR::Coefficients<double>>& lpCoeffs, std::vector<IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate)
{
    // Setup Ripple (1.0 dB is a standard, musical choice for steepness)
    const double rippleDB = 1.0;
    const double epsilon = std::sqrt(std::pow(10.0, rippleDB / 10.0) - 1.0);
    const double mu = (1.0 / filterOrder) * std::asinh(1.0 / epsilon);
    const double a_ellipse = std::sinh(mu);
    const double b_ellipse = std::cosh(mu);

    // Pre-warp the target frequencies for the Bilinear Transform
    const double lpW = std::tan(MathConstants<double>::pi * lpCutoff / sampleRate);
    const double hpW = std::tan(MathConstants<double>::pi * hpCutoff / sampleRate);

    // Loop through the pole pairs (Each pair creates one biquad)
    int numBiquads = filterOrder / 2;
    for (int k = 1; k <= numBiquads; ++k)
    {
        // Calculate angle for this specific pole
        double angle = (MathConstants<double>::pi * (2.0 * k - 1.0)) / (2.0 * filterOrder);

        // Normalized Analog Poles (s-plane)
        double sigma = -a_ellipse * std::sin(angle);
        double omega = b_ellipse * std::cos(angle);
        std::complex<double> p(sigma, omega); // The normalized analog pole

        // ==========================================
        // LOW PASS BIQUAD
        // ==========================================
        // Scale analog pole by target frequency
        std::complex<double> lpSPole = p * lpW;

        // Bilinear Transform (s -> z)
        std::complex<double> lpZPole = (1.0 + lpSPole) / (1.0 - lpSPole);

        double lpA1 = -2.0 * lpZPole.real();
        double lpA2 = std::norm(lpZPole); // norm() in C++ returns magnitude squared

        double lpB0 = 1.0, lpB1 = 2.0, lpB2 = 1.0;

        // Normalize gain at DC (z = 1)
        double lpGainAtDC = (lpB0 + lpB1 + lpB2) / (1.0 + lpA1 + lpA2);
        lpB0 /= lpGainAtDC;
        lpB1 /= lpGainAtDC;
        lpB2 /= lpGainAtDC;

        // ==========================================
        // HIGH PASS BIQUAD
        // ==========================================
        // High Pass mapping: s -> (hpW / s)
        std::complex<double> hpSPole = hpW / p;

        // Bilinear Transform (s -> z)
        std::complex<double> hpZPole = (1.0 + hpSPole) / (1.0 - hpSPole);

        double hpA1 = -2.0 * hpZPole.real();
        double hpA2 = std::norm(hpZPole);

        double hpB0 = 1.0, hpB1 = -2.0, hpB2 = 1.0;

        // Normalize gain at Nyquist (z = -1)
        double hpGainAtNyquist = (hpB0 - hpB1 + hpB2) / (1.0 - hpA1 + hpA2);
        hpB0 /= hpGainAtNyquist;
        hpB1 /= hpGainAtNyquist;
        hpB2 /= hpGainAtNyquist;

        // ==========================================
        // OVERALL GAIN CORRECTION (The Pro Secret)
        // ==========================================
        // Even-order Chebyshev filters naturally have a dip at DC (LP) or Nyquist (HP).
        // If we normalize EVERY biquad to 1.0, the "ripples" will actually peak at +1dB,
        // which can cause clipping. We scale the very first biquad down by the ripple 
        // amount so the maximum peak of the filter stays perfectly at 0dB.
        if (k == 1) {
            double scale = std::pow(10.0, -rippleDB / 20.0);
            lpB0 *= scale; lpB1 *= scale; lpB2 *= scale;
            hpB0 *= scale; hpB1 *= scale; hpB2 *= scale;
        }

        // Push to vectors
        lpCoeffs.emplace_back(
            lpB0, lpB1, lpB2,
            1.0, lpA1, lpA2);

        hpCoeffs.emplace_back(
            hpB0, hpB1, hpB2,
            1.0, hpA1, hpA2);
    }
}

void IIRFilterAudioProcessor::chebyshevIICoefficients(std::vector<IIR::Coefficients<double>>& lpCoeffs, std::vector<IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate)
{
    // Setup Stopband Attenuation (e.g., 40dB of rejection)
    const double stopbandAttenDB = 40.0 + 10.0 * filterOrder/2;

    // The epsilon calculation is inverted for Type II
    const double epsilon = 1.0 / std::sqrt(std::pow(10.0, stopbandAttenDB / 10.0) - 1.0);
    const double mu = (1.0 / filterOrder) * std::asinh(1.0 / epsilon);
    const double a_ellipse = std::sinh(mu);
    const double b_ellipse = std::cosh(mu);

    // Pre-warp frequencies
    const double lpW = std::tan(MathConstants<double>::pi * lpCutoff / sampleRate);
    const double hpW = std::tan(MathConstants<double>::pi * hpCutoff / sampleRate);

    int numBiquads = filterOrder / 2;
    for (int k = 1; k <= numBiquads; ++k)
    {
        double angle = (MathConstants<double>::pi * (2.0 * k - 1.0)) / (2.0 * filterOrder);

        // --- Calculate Analog Poles and Zeros ---

        // Base Type I Pole
        double sigma = -a_ellipse * std::sin(angle);
        double omega = b_ellipse * std::cos(angle);
        std::complex<double> basePole(sigma, omega);

        // Type II Pole is the inverse of the Type I pole
        // 1 / (sigma + j*omega)
        std::complex<double> analogPole = 1.0 / basePole;

        // Type II Zeros lie strictly on the imaginary axis
        // z = j / cos(angle)
        double zeroOmega = 1.0 / std::cos(angle);
        std::complex<double> analogZero(0.0, zeroOmega);

        // ==========================================
        // LOW PASS BIQUAD
        // ==========================================
        std::complex<double> lpSPole = analogPole * lpW;
        std::complex<double> lpSZero = analogZero * lpW;

        // Bilinear Transform for Poles and Zeros
        std::complex<double> lpZPole = (1.0 + lpSPole) / (1.0 - lpSPole);
        std::complex<double> lpZZero = (1.0 + lpSZero) / (1.0 - lpSZero);

        double lpA1 = -2.0 * lpZPole.real();
        double lpA2 = std::norm(lpZPole); // Magnitude squared

        double lpB0 = 1.0;
        double lpB1 = -2.0 * lpZZero.real();
        double lpB2 = std::norm(lpZZero);    // Should be exactly 1.0 for purely imaginary zeros

        // Normalize gain at DC (z = 1) for Low Pass
        double lpGainAtDC = (lpB0 + lpB1 + lpB2) / (1.0 + lpA1 + lpA2);
        lpB0 /= lpGainAtDC;
        lpB1 /= lpGainAtDC;
        lpB2 /= lpGainAtDC;

        // ==========================================
        // HIGH PASS BIQUAD
        // ==========================================
        // HP mapping: s -> (W / s)
        std::complex<double> hpSPole = hpW / analogPole;
        std::complex<double> hpSZero = hpW / analogZero;

        std::complex<double> hpZPole = (1.0 + hpSPole) / (1.0 - hpSPole);
        std::complex<double> hpZZero = (1.0 + hpSZero) / (1.0 - hpSZero);

        double hpA1 = -2.0 * hpZPole.real();
        double hpA2 = std::norm(hpZPole);

        double hpB0 = 1.0;
        double hpB1 = -2.0 * hpZZero.real();
        double hpB2 = std::norm(hpZZero);

        // Normalize gain at Nyquist (z = -1) for High Pass
        double hpGainAtNyquist = (hpB0 - hpB1 + hpB2) / (1.0 - hpA1 + hpA2);
        hpB0 /= hpGainAtNyquist;
        hpB1 /= hpGainAtNyquist;
        hpB2 /= hpGainAtNyquist;

        // Push to vectors
        lpCoeffs.emplace_back(
            lpB0, lpB1, lpB2,
            1.0f, lpA1, lpA2);

        hpCoeffs.emplace_back(
            hpB0, hpB1, hpB2,
            1.0, hpA1, hpA2);
    }
}

void IIRFilterAudioProcessor::ellipticCoefficients(std::vector<IIR::Coefficients<double>>& lpCoeffs, std::vector<IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate)
{
    // Fetch the correct prototype roots for this specific order
    auto proto = getEllipticProto(filterOrder);

    // Pre-warp frequencies
    double lpW = std::tan(MathConstants<double>::pi * lpCutoff / sampleRate);
    double hpW = std::tan(MathConstants<double>::pi * hpCutoff / sampleRate);

    // Assuming our prototype was generated with 1.0 dB passband ripple
    const double rippleDB = 2.0;
    int stageIndex = 0;

    // Loop over the returned roots (1 loop for 2nd order, 4 loops for 8th order)
    for (const auto& root : proto)
    {
        // =======================
        // LOW PASS CALCULATION
        // =======================
        std::complex<double> lpSPole = root.pole * lpW;
        std::complex<double> lpSZero = root.zero * lpW;

        std::complex<double> lpZPole = (1.0 + lpSPole) / (1.0 - lpSPole);
        std::complex<double> lpZZero = (1.0 + lpSZero) / (1.0 - lpSZero);

        double lpA1 = -2.0 * lpZPole.real();
        double lpA2 = std::norm(lpZPole);

        double lpB0 = 1.0;
        double lpB1 = -2.0 * lpZZero.real();
        double lpB2 = std::norm(lpZZero);

        // Normalize DC Gain to 0dB (z = 1)
        double lpGain = (lpB0 + lpB1 + lpB2) / (1.0 + lpA1 + lpA2);
        lpB0 /= lpGain; lpB1 /= lpGain; lpB2 /= lpGain;

        // =======================
        // HIGH PASS CALCULATION
        // =======================
        std::complex<double> hpSPole = hpW / root.pole;
        std::complex<double> hpSZero = hpW / root.zero;

        std::complex<double> hpZPole = (1.0 + hpSPole) / (1.0 - hpSPole);
        std::complex<double> hpZZero = (1.0 + hpSZero) / (1.0 - hpSZero);

        double hpA1 = -2.0 * hpZPole.real();
        double hpA2 = std::norm(hpZPole);

        double hpB0 = 1.0;
        double hpB1 = -2.0 * hpZZero.real();
        double hpB2 = std::norm(hpZZero);

        // Normalize Nyquist Gain to 0dB (z = -1)
        double hpGain = (hpB0 - hpB1 + hpB2) / (1.0 - hpA1 + hpA2);
        hpB0 /= hpGain; hpB1 /= hpGain; hpB2 /= hpGain;

        // ==========================================
        // OVERALL GAIN CORRECTION (Ripple Check)
        // ==========================================
        // Even-order Elliptic filters start in a "trough" of the ripple at DC.
        // If we normalize every stage to 0dB at DC, the peaks will jump to +1dB.
        // We scale the very first biquad down to keep the maximum peak at exactly 0dB.
        if (stageIndex == 0) {
            double scale = std::pow(10.0, -rippleDB / 20.0);
            lpB0 *= scale; lpB1 *= scale; lpB2 *= scale;
            hpB0 *= scale; hpB1 *= scale; hpB2 *= scale;
        }
        stageIndex++;

        // Push final coefficients to the vectors
        lpCoeffs.emplace_back(
            lpB0, lpB1, lpB2,
            1.0, lpA1, lpA2);

        hpCoeffs.emplace_back(
            hpB0, hpB1, hpB2,
            1.0, hpA1, hpA2);
    }
}

// =========================================================
// ELLIPTIC LOOKUP TABLE (Normalized for 1dB Pass / 50dB Stop)
// =========================================================
std::vector<IIRFilterAudioProcessor::Root> IIRFilterAudioProcessor::getEllipticProto(int filterOrder) {
    std::vector<IIRFilterAudioProcessor::Root> roots;
    switch (filterOrder) {
    case 2:
        roots.push_back({ std::complex<double>(-0.5478900655, 0.8967136436), std::complex<double>(0.0, 17.6416573870) });
        break;
    case 4:
        roots.push_back({ std::complex<double>(-0.3528715618, 0.4466760060), std::complex<double>(0.0, 2.0425534127) });
        roots.push_back({ std::complex<double>(-0.1194893324, 0.9897673273), std::complex<double>(0.0, 4.6644397106) });
        break;
    case 6:
        roots.push_back({ std::complex<double>(-0.2896601785, 0.3601789200), std::complex<double>(0.0, 1.2215575156) });
        roots.push_back({ std::complex<double>(-0.1364518242, 0.8345761842), std::complex<double>(0.0, 1.4934211275) });
        roots.push_back({ std::complex<double>(-0.0331836089, 0.9983145524), std::complex<double>(0.0, 3.5928449672) });
        break;
    case 8:
        roots.push_back({ std::complex<double>(-0.2721701727, 0.3379520000), std::complex<double>(0.0, 1.0582200077) });
        roots.push_back({ std::complex<double>(-0.1311677395, 0.7858076585), std::complex<double>(0.0, 1.1194508835) });
        roots.push_back({ std::complex<double>(-0.0431380175, 0.9526164553), std::complex<double>(0.0, 1.3904007784) });
        roots.push_back({ std::complex<double>(-0.0093822327, 0.9996289522), std::complex<double>(0.0, 3.3588850329) });
        break;
    default:
        break;
    }
    return roots;
}

void IIRFilterAudioProcessor::besselCoefficients(std::vector<IIR::Coefficients<double>>& lpCoeffs, std::vector<IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate)
{
    int besselType = static_cast<int>(parameters.getRawParameterValue("approximation")->load());
	double internalSampleRate = sampleRate;

    if (besselType == 4) {
        internalSampleRate = sampleRate * 4.0;

        // Adjust the frequencies to match the Bessel prototype's definition of "cutoff" at -3dB
        double kn = 1.0;
        switch (filterOrder) {
        case 2: // 1 Biquad(s)
            kn = 0.734400887073410;
            break;
        case 4: // 2 Biquad(s)
            kn = 0.473055319257907;
            break;
        case 6: // 3 Biquad(s)
            kn = 0.369905240484611;
            break;
        case 8: // 4 Biquad(s)
            kn = 0.314503264273131;
            break;
        }

        hpCutoff /= kn;
        lpCutoff *= kn;
    }

    // Fetch the Phase-Normalized Bessel poles
    auto proto = getBesselProto(filterOrder);

    // Pre-warp frequencies for Bilinear Transform
    double lpW = std::tan(MathConstants<double>::pi * lpCutoff / internalSampleRate);
    double hpW = std::tan(MathConstants<double>::pi * hpCutoff / internalSampleRate);

    for (const auto& root : proto)
    {
        // =======================
        // LOW PASS CALCULATION
        // =======================
        // Bessel zeros are at s = infinity, which maps to z = -1
        std::complex<double> lpSPole = root.pole * lpW;
        std::complex<double> lpZPole = (1.0 + lpSPole) / (1.0 - lpSPole);

        double lpA1 = -2.0 * lpZPole.real();
        double lpA2 = std::norm(lpZPole);

        // Standard Low-Pass Zeros: (1 + z^(-1))^2
        double lpB0 = 1.0;
        double lpB1 = 2.0;
        double lpB2 = 1.0;

        // Normalize DC Gain (z = 1) to 0dB
        double lpGain = (lpB0 + lpB1 + lpB2) / (1.0 + lpA1 + lpA2);
        lpB0 /= lpGain; lpB1 /= lpGain; lpB2 /= lpGain;

        lpCoeffs.emplace_back(
            lpB0, lpB1, lpB2,
            1.0, lpA1, lpA2);

        // =======================
        // HIGH PASS CALCULATION
        // =======================
        // Bessel High-Pass is calculated by inverting the S-plane pole (s -> 1/s)
        // High-Pass zeros are at s = 0, which maps to z = 1
        std::complex<double> hpSPole = hpW / root.pole;
        std::complex<double> hpZPole = (1.0 + hpSPole) / (1.0 - hpSPole);

        double hpA1 = -2.0 * hpZPole.real();
        double hpA2 = std::norm(hpZPole);

        // Standard High-Pass Zeros: (1 - z^(-1))^2
        double hpB0 = 1.0;
        double hpB1 = -2.0;
        double hpB2 = 1.0;

        // Normalize Nyquist Gain (z = -1) to 0dB
        double hpGain = (hpB0 - hpB1 + hpB2) / (1.0 - hpA1 + hpA2);
        hpB0 /= hpGain; hpB1 /= hpGain; hpB2 /= hpGain;

        hpCoeffs.emplace_back(
            hpB0, hpB1, hpB2,
            1.0, hpA1, hpA2);
    }
}

// ===================
// BESSEL LOOKUP TABLE
// ===================
std::vector<IIRFilterAudioProcessor::Root> IIRFilterAudioProcessor::getBesselProto(int filterOrder)
{
    std::vector<IIRFilterAudioProcessor::Root> roots;
    int besselType = static_cast<int>(parameters.getRawParameterValue("approximation")->load());
    if (besselType == 4) {
        // --- Bessel Analog Prototype Poles (Phase Normalized) ---
		switch (filterOrder) {
        case 2: // 1 Biquad(s)
            roots.push_back({ std::complex<double>(-1.500000000000000, 0.866025403784439), std::complex<double>(0.0, 0.0) });
            break;

        case 4: // 2 Biquad(s)
            roots.push_back({ std::complex<double>(-2.896210602820372, 0.867234128934503), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-2.103789397179627, 2.657418041856752), std::complex<double>(0.0, 0.0) });
            break;

        case 6: // 3 Biquad(s)
            roots.push_back({ std::complex<double>(-4.248359395863364, 0.867509673231366), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-3.735708356325814, 2.626272311447126), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-2.515932247810821, 4.492672953653943), std::complex<double>(0.0, 0.0) });
            break;

        case 8: // 4 Biquad(s)
            roots.push_back({ std::complex<double>(-5.587886043263086, 0.867614445352787), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-5.204840790636881, 2.616175152642527), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-4.368289217202402, 4.414442500471539), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-2.838983948897632, 6.353911298604878), std::complex<double>(0.0, 0.0) });
            break;
        }
    }

    else if (besselType == 5) {
        // --- Frequency-Normalized Bessel Prototype Poles (-3dB at 1 rad/s) ---
        switch (filterOrder) {
        case 2: // 1 Biquad(s)
            roots.push_back({ std::complex<double>(-1.101601330592161, 0.636009824757034), std::complex<double>(0.0, 0.0) });
            break;

        case 4: // 2 Biquad(s)
            roots.push_back({ std::complex<double>(-1.370067830551442, 0.410249717493751), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-0.995208764350272, 1.257105739454664), std::complex<double>(0.0, 0.0) });
            break;

        case 6: // 3 Biquad(s)
            roots.push_back({ std::complex<double>(-1.571490403616031, 0.320896374222624), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-1.381858097596563, 0.971471890711571), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-0.930656522946859, 1.661863268942591), std::complex<double>(0.0, 0.0) });
            break;

        case 8: // 4 Biquad(s)
            roots.push_back({ std::complex<double>(-1.757408400401652, 0.272867575102233), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-1.636939418126887, 0.822795625139699), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-1.373841217637376, 1.388356575877562), std::complex<double>(0.0, 0.0) });
            roots.push_back({ std::complex<double>(-0.892869718847137, 1.998325843641306), std::complex<double>(0.0, 0.0) });
            break;
        }
    }
    return roots;
}

AudioProcessorValueTreeState::ParameterLayout IIRFilterAudioProcessor::createParameterLayout() {
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<AudioParameterFloat>("lpCutoff", "Low Pass Cutoff Frequency", NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 20000.f, AudioParameterFloatAttributes()));
    layout.add(std::make_unique<AudioParameterFloat>("hpCutoff", "High Pass Cutoff Frequency", NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 10.f, AudioParameterFloatAttributes()));
    layout.add(std::make_unique<AudioParameterFloat>(
        "filterOrder",
        "Filter FilterOrder",
        NormalisableRange<float>(2.f, 8.f, 2.f),
        2.f,
        AudioParameterFloatAttributes().withLabel("Order").withStringFromValueFunction([](float value, int) {
            return String(static_cast<int>(value));
        }).withValueFromStringFunction([](const String& text) {
            return static_cast<float>(text.getIntValue());
			})
    ));
    layout.add(std::make_unique<AudioParameterChoice>(
        "approximation",
        "Filter Approximation Type",
        StringArray{ "Butterworth", "Chebyshev I", "Chebyshev II", "Elliptic", "Bessel 0Hz", "Bessel -3dB"},
        0
	));
    layout.add(std::make_unique<juce::AudioParameterBool>("bypassHp", "Bypass HP", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("bypassLp", "Bypass LP", false));

    return layout;
}

//==============================================================================
bool IIRFilterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* IIRFilterAudioProcessor::createEditor()
{
    return new IIRFilterAudioProcessorEditor (*this);
}

//==============================================================================
void IIRFilterAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // Create an XML element to hold your data
    auto state = parameters.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());

    // Convert that XML to a binary block for the DAW
    copyXmlToBinary(*xml, destData);
}

void IIRFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Convert the binary block back into XML
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        // Check if the XML tag matches your ValueTree name
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            // Update the APVTS, which automatically updates your sliders
            parameters.replaceState(ValueTree::fromXml(*xmlState));

			// Update the filter coefficients based on the restored parameters
            updateCoefficients(getSampleRate());
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IIRFilterAudioProcessor();
}
