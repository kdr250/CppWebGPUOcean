struct Particle {
    position: vec3f, 
    v: vec3f, 
    force: vec3f, 
    density: f32, 
    nearDensity: f32, 
}

struct SPHParams {
    mass: f32, 
    kernelRadius: f32, 
    kernelRadiusPow2: f32, 
    kernelRadiusPow5: f32, 
    kernelRadiusPow6: f32,  
    kernelRadiusPow9: f32, 
    dt: f32, 
    stiffness: f32, 
    nearStiffness: f32, 
    restDensity: f32, 
    viscosity: f32, 
    n: u32
}

@group(0) @binding(0) var<storage, read> sourceParticles: array<Particle>;
@group(0) @binding(1) var<storage, read_write> targetParticles: array<Particle>;
@group(0) @binding(2) var<storage, read> cellParticleCount : array<u32>;
@group(0) @binding(3) var<storage, read> particleCellOffset : array<u32>;
@group(0) @binding(4) var<uniform> env : Environment;
@group(0) @binding(5) var<uniform> params : SPHParams;

struct Environment {
    grids: vec3i,
    cellSize: f32, 
    half: vec3f,
    offset: f32, 
}

fn cellId(position: vec3f) -> i32 {
    let xi: i32 = i32(floor((position.x + env.half.x + env.offset) / env.cellSize));
    let yi: i32 = i32(floor((position.y + env.half.y + env.offset) / env.cellSize));
    let zi: i32 = i32(floor((position.z + env.half.z + env.offset) / env.cellSize));

    return xi + yi * env.grids.x + zi * env.grids.x * env.grids.y;
}

@compute
@workgroup_size(64)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
    if (id.x < params.n) {
        let cellId: i32 = cellId(sourceParticles[id.x].position);
        // TODO : 変える
        if (cellId < env.grids.x * env.grids.y * env.grids.z) {
            let targetIndex = cellParticleCount[cellId + 1] - particleCellOffset[id.x] - 1;
            if (targetIndex < params.n) {
                targetParticles[targetIndex] = sourceParticles[id.x];
            }
        }
    }
}
