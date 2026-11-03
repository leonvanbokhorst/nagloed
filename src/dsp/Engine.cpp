#include "Engine.h"
#include "EngineDSP.h"

namespace nagloed
{

struct Engine::Impl : public EngineDSP
{
};

Engine::Engine()  : impl (std::make_unique<Impl>()) {}
Engine::~Engine() = default;

void Engine::prepare (double sr, int maxBlock, int numChannels)
{
    impl->prepare (sr, maxBlock, numChannels);
}

void Engine::reset() { impl->reset(); }

void Engine::setParams (const EngineParams& p) noexcept { impl->setParams (p); }

void Engine::process (juce::AudioBuffer<float>& io, const juce::MidiBuffer& midi) noexcept
{
    impl->process (io, midi);
}

void Engine::getVisual (VisualState& out) const noexcept { impl->getVisual (out); }

int Engine::getLatencySamples() const noexcept { return impl->getLatencySamples(); }

} // namespace nagloed
