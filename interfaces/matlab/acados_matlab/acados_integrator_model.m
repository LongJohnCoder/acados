classdef acados_integrator_model < handle
	


	properties
		name
		type
		expr
		sym_x
		sym_u
		sym_xdot
		sym_z
		nx
		nu
		nz
		T
		model_struct
	end %properties



	methods
		

		function obj = acados_integrator_model()
			obj.name = 'model';
			obj.type = 0;
			obj.expr = 0;
			obj.sym_x = 0;
			obj.sym_u = 0;
			obj.sym_xdot = 0;
			obj.sym_z = 0;
			obj.nx = 0;
			obj.nu = 0;
			obj.nz = 0;
			obj.T = 0;
			obj.model_struct = struct;
			obj.model_struct.name = obj.name;
		end


		function obj = set(obj, field, value)
			if (strcmp(field, 'type'))
				obj.type = value;
				obj.model_struct.type = value;
			elseif (strcmp(field, 'expr'))
				obj.expr = value;
				obj.model_struct.expr = value;
			elseif (strcmp(field, 'sym_x'))
				obj.sym_x = value;
				obj.model_struct.sym_x = value;
			elseif (strcmp(field, 'sym_xdot'))
				obj.sym_xdot = value;
				obj.model_struct.sym_xdot = value;
			elseif (strcmp(field, 'sym_u'))
				obj.sym_u = value;
				obj.model_struct.sym_u = value;
			elseif (strcmp(field, 'sym_z'))
				obj.sym_z = value;
				obj.model_struct.sym_z = value;
			elseif (strcmp(field, 'nx'))
				obj.nx = value;
				obj.model_struct.nx = value;
			elseif (strcmp(field, 'nu'))
				obj.nu = value;
				obj.model_struct.nu = value;
			elseif (strcmp(field, 'nz'))
				obj.nz = value;
				obj.model_struct.nz = value;
			elseif (strcmp(field, 'T'))
				obj.T = value;
				obj.model_struct.T = value;
			else
				disp('acados_integrator_model: set: wrong field');
			end
		end

	end % methods



end % class

