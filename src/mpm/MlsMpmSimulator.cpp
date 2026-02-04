#include "MlsMpmSimulator.h"

#include "../Application.h"

MlsMpmSimulator::MlsMpmSimulator(wgpu::Buffer particleBuffer,
                                 wgpu::Buffer posvelBuffer,
                                 float renderDiameter,
                                 wgpu::Device device)
{
}

void MlsMpmSimulator::Compute(wgpu::CommandEncoder commandEncoder) {}

void MlsMpmSimulator::Reset(int numParticles,
                            const glm::vec3& initHalfBoxSize,
                            RenderUniforms& renderUniforms)
{
}

void MlsMpmSimulator::ChangeBoxSize(const glm::vec3& realBoxSize) {}
