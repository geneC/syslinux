-- get nice output
printf = function(s,...)
           return io.write(s:format(...))
         end

-- list available vesa modes
-- only one supported right now, not of much use
modes = vesa.getmodes()

for mind,mode in pairs(modes) do
   printf("%04x: %dx%dx%d\n", mode['mode'], mode['hres'], mode['vres'], mode['bpp'])
end

printf("Hello World! - text mode")

-- lets go to graphics land
vesa.setmode()

printf("Hello World! - VESA mode")

syslinux.sleep(1)

-- some text to display "typing style"
textline=[[

From syslinux GSOC 2009 home page:

Finish the Lua engine

We already have a Lua interpreter integrated with the Syslinux build. However, right now it is not very useful. We need to create a set of bindings to the Syslinux functionality, and have an array of documentation and examples so users can use them.

This is not a documentation project, but the documentation deliverable will be particularly important for this one, since the intended target is system administrators, not developers. 
]]


-- do display loop
-- keep in mind: background change will not erase text!
while ( true ) do

vesa.load_background("/PXE-RRZE_small.jpg")

syslinux.sleep(1)

for i = 1, #textline do
    local c = textline:sub(i,i)
    printf("%s", c)
    syslinux.msleep(200)
end

syslinux.sleep(10)

vesa.load_background("/sample2.jpg")
syslinux.sleep(10)

end
