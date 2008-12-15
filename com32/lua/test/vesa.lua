-- get nice output
printf = function(s,...)
           return io.write(s:format(...))
         end

modes = vesa.getmodes()

for mind,mode in pairs(modes) do
   printf("%04x: %dx%dx%d\n", mode['mode'], mode['hres'], mode['vres'], mode['bpp'])
end

