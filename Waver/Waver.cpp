#include <iostream>

#include "olcNoiseMaker.h"

struct sEnvelopeADSR {
    double dAttackTime;
    double dDecayTime;
    double dReleaseTime;

    double dSustainAmplitude;
    double dStartAmplitude;

    double dTriggerOnTime;
    double dTriggerOffTime;

    bool bNoteOn;

    sEnvelopeADSR() 
    {
        dAttackTime = 0.025;
        dDecayTime = 0.25;
        dStartAmplitude = 0.75;
        dSustainAmplitude = 1.0;
        dReleaseTime = 0.6;
        dTriggerOffTime = 0.0;
        dTriggerOnTime = 0.0;
        bNoteOn = false;
    }

    double GetAmplitude(double dTime) {
        double dAmplitude = 0.0;
        double dLifeTime = dTime - dTriggerOnTime;

        if (bNoteOn) {
            //Attack
            if (dLifeTime <= dAttackTime)
                dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

            //Decay
            if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
                dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

            //Sustain
            if (dLifeTime > (dAttackTime + dDecayTime)) {
                dAmplitude = dSustainAmplitude;
            }
        }
        else {
            //Release
            dAmplitude = ((dTime - dTriggerOffTime) / dReleaseTime) * (0.0 - dSustainAmplitude) + dSustainAmplitude;
        }

        if (dAmplitude <= 0.0001) {
            dAmplitude = 0;
        }

        return dAmplitude;
    }

    void NoteOn(double dTimeOn) {
        dTriggerOnTime = dTimeOn;

        bNoteOn = true;
    }

    void NoteOff(double dTimeOff) {
        dTriggerOffTime = dTimeOff;

        bNoteOn = false;
    }
};

sEnvelopeADSR envelope;

atomic<double> dFrequencyOutput = 0.0;

double w(double dHertz) {
    return dHertz * 2.0 * PI;
}

double osc(double dHertz, double dTime, int nType, double dLFOHertz = 0.0, double dLFOAmplitude = 0.0) {
    
    double dFreq = w(dHertz) * dTime + dLFOAmplitude * dHertz * sin(w(dLFOHertz) * dTime);

    switch (nType)
    {
    case 0:
        return sin(dFreq);

    case 1:
        return sin(dFreq) > 0.0 ? 1.0 : -1.0;

    case 2:
        return asin(sin(dFreq)) * 2.0 / PI;

    case 3:
        return (2.0 * PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0));

    default:
        return 0.0;
    }
}

struct Instrument {
    double dVolume = 1.0;
    sEnvelopeADSR env;

    virtual double Sound(double dTime, double dFrequency) = 0;
};

struct GhostInstrument : public Instrument {

    GhostInstrument() {
        env.dAttackTime = 0.025;
        env.dDecayTime = 0.25;
        env.dStartAmplitude = 0.75;
        env.dSustainAmplitude = 1.0;
        env.dReleaseTime = 0.6;
    }

    virtual double Sound(double dTime, double dFrequency) override {
        double dOutput = env.GetAmplitude(dTime) * (osc(dFrequency * 1.0, dTime, 0, 5.0, 0.01));

        return dOutput;
    }
};

Instrument* voice = nullptr;

double MakeNoise(double dTime)
{
    double dOutput = voice->Sound(dTime, dFrequencyOutput);

    return dOutput * 0.5;
}

int main()
{
    std::vector<std::wstring> devices = olcNoiseMaker<short>::Enumerate();

    for (auto d : devices) std::wcout << "Found Output Device: " << d << std::endl;

    olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 512);

    sound.SetUserFunction(MakeNoise);

    double dOctaveBaseFrequency = 220.0;
    double d12thRootOf2 = pow(2.0, 1.0 / 12.0);

    voice = new GhostInstrument();

    int nCurrentKey = -1;
    bool bKeyPressed = false;

    while (true) {
        bool bKeyPressed = false;

        for (int i = 0; i < 16; i++) {
            if ( GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[i])) & 0x8000) {
                if (nCurrentKey != i) {
                    dFrequencyOutput = dOctaveBaseFrequency * pow(d12thRootOf2, i);
                    voice->env.NoteOn(sound.GetTime());
                    nCurrentKey = i;
                }

                bKeyPressed = true;
            }
        }

        if (!bKeyPressed) {
            if (nCurrentKey != -1) {
                nCurrentKey = -1;
                voice->env.NoteOff(sound.GetTime());
            }
        }
    }
    
    return 0;
}