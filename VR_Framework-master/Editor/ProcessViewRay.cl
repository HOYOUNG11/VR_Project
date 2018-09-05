struct point {
	float x;
  float y;
  float z;
};

struct ray {
	struct point start;
	float x;
	float y;
	float z;
};

float tri_interpolation(struct point data, __global short* raw_data, int index, __global int* minimum, __global int*maximum) {
	float ix1 = (data.x - (int)data.x) * raw_data[index] + ((int)data.x - data.x + 1) * raw_data[index + 1];
	float ix2 = (data.x - (int)data.x) * raw_data[index + 512] + ((int)data.x - data.x + 1) * raw_data[index + 513];
	float ix3 = (data.x - (int)data.x) * raw_data[index + 262144] + ((int)data.x - data.x + 1) * raw_data[index + 262145];
	float ix4 = (data.x - (int)data.x) * raw_data[index + 262656] + ((int)data.x - data.x + 1) * raw_data[index + 262657];

	float iy1 = (data.y - (int)data.y) * ix1 + ((int)data.y - data.y + 1) * ix3;
	float iy2 = (data.y - (int)data.y) * ix2 + ((int)data.y - data.y + 1) * ix4;

	float pixel = (data.z - (int)data.z) * iy1 + ((int)data.z - data.z + 1) * iy2;

	if (pixel < (*minimum)) { return 0; }
	if (pixel > (*maximum)) { return 1; }

	return (pixel - (*minimum)) / ((*maximum) - (*minimum));
}
__kernel void ProcessImageView(__global struct point* data,
															 __global struct point* end,
															 __global struct ray* ray1,
															 __global struct ray* ray2,
														 	 __global struct ray* view_ray,
														 	 __global int* minimum,
														 	 __global int* maximum,
														 	 __global short* raw_data,
														 	 __global unsigned char* RGB_converted,
														   __global float* watch) {
  size_t id = (get_global_id(1) * get_global_size(0)) + get_global_id(0);

	float convert = (float)56 / (float)512;

  data[id].x = end->x + get_global_id(0) * ray1->x + get_global_id(1) * ray2->x;
  data[id].y = end->y + get_global_id(0) * ray1->y + get_global_id(1) * ray2->y;
  data[id].z = end->z + get_global_id(0) * ray1->z * convert + get_global_id(1) * ray2->z * convert;

	float alpha = 0;
	float color = 0;


	for (int i=0; i<2000; i++) {
		data[id].x += view_ray->x;
		data[id].y += view_ray->y;
		data[id].z += view_ray->z;

		if ((data[id].x >= 0 && data[id].x <= 512)
		&& (data[id].y >= 0 && data[id].y <= 512)
		&& (data[id].z >= 0 && data[id].z <= 56)) {
			int index = (int)data[id].x + 512 * (int)data[id].y + 262144 * (int)data[id].z;
			if (index <= 14680064 && index >= 0) {

				float pixel_converted = tri_interpolation(data[id], raw_data, index, minimum,maximum);

				alpha = alpha + (1 - alpha) * pixel_converted;
				color = color + (1 - alpha) * pixel_converted;
				if (alpha >= 0.95) {  break; }
			}
		}
	}
	watch[id] = alpha;
	RGB_converted[id * 4] = (unsigned char)(color * 255);
	RGB_converted[id * 4 + 1] = (unsigned char)(color * 255);
	RGB_converted[id * 4 + 2] = (unsigned char)(color * 255);
	RGB_converted[id * 4 + 3] = (unsigned char)(alpha * 255);
}
