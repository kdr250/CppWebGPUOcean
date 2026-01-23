struct Particle {
    position: vec3f, 
    v: vec3f, 
    force: vec3f, 
    density: f32, 
    nearDensity: f32, 
}

struct Environment {
    grids: vec3i,
    cellSize: f32, 
    half: vec3f,
    offset: f32, 
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

@group(0) @binding(0) var<storage, read_write> particles: array<Particle>;
@group(0) @binding(1) var<storage, read> sortedParticles: array<Particle>;
@group(0) @binding(2) var<storage, read> prefixSum: array<u32>;
@group(0) @binding(3) var<uniform> env: Environment;
@group(0) @binding(4) var<uniform> params: SPHParams;

fn nearDensityKernel(r: f32) -> f32 {
    let scale = 15.0 / (3.1415926535 * params.kernelRadiusPow6);
    let d = params.kernelRadius - r;
    return scale * d * d * d;
}

fn densityKernel(r: f32) -> f32 {
    let scale = 315.0 / (64. * 3.1415926535 * params.kernelRadiusPow9);
    let dd = params.kernelRadiusPow2 - r * r;
    return scale * dd * dd * dd;
}

fn cellPosition(v: vec3f) -> vec3i {
    let xi = i32(floor((v.x + env.half.x + env.offset) / env.cellSize));
    let yi = i32(floor((v.y + env.half.y + env.offset) / env.cellSize));
    let zi = i32(floor((v.z + env.half.z + env.offset) / env.cellSize));
    return vec3i(xi, yi, zi);
}

fn cellNumberFromId(xi: i32, yi: i32, zi: i32) -> i32 {
    return xi + yi * env.grids.x + zi * env.grids.x * env.grids.y;
}

@compute @workgroup_size(64)
fn computeDensity(@builtin(global_invocation_id) id: vec3<u32>) {
    if (id.x < params.n) {
        particles[id.x].density = 0.0;
        particles[id.x].nearDensity = 0.0;
        let pos_i = particles[id.x].position;
        let n = params.n;

        let v = cellPosition(pos_i);
        if (v.x < env.grids.x && 0 <= v.x && 
            v.y < env.grids.y && 0 <= v.y && 
            v.z < env.grids.z && 0 <= v.z) 
        {
            for (var dz = max(-1, -v.z); dz <= min(1, env.grids.z - v.z - 1); dz++) {
                for (var dy = max(-1, -v.y); dy <= min(1, env.grids.y - v.y - 1); dy++) {
                    let dxMin = max(-1, -v.x);
                    let dxMax = min(1, env.grids.x - v.x - 1);
                    let startCellNum = cellNumberFromId(v.x + dxMin, v.y + dy, v.z + dz);
                    let endCellNum = cellNumberFromId(v.x + dxMax, v.y + dy, v.z + dz);
                    let start = prefixSum[startCellNum];
                    let end = prefixSum[endCellNum + 1];
                    for (var j = start; j < end; j++) {
                        let pos_j = sortedParticles[j].position;
                        let r2 = dot(pos_i - pos_j, pos_i - pos_j);
                        if (r2 < params.kernelRadiusPow2) {
                            particles[id.x].density += params.mass * densityKernel(sqrt(r2));
                            particles[id.x].nearDensity += params.mass * nearDensityKernel(sqrt(r2));
                        }
                    }
                }
            }
        }
    }
}
