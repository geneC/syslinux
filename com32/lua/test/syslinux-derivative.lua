-- get nice output
printf = function(s,...)
           return io.write(s:format(...))
         end

-- get syslinux derivative (ISOLINUX, PXELINUX, SYSLINUX)
derivative = syslinux.derivative()

printf("Run specific command depending on the Syslinux derivate:\n")
printf("--------------------------------------------------------\n\n")
printf("  Detected Syslinux derivative: %s\n", derivative)

if derivative == "SYSLINUX" then
	-- swap internal (hd1) hard drive with USB stick (hd0)
	commandline = 'chain.c32 hd1 swap'
elseif derivative == "ISOLINUX" then
	-- boot first hard drive
	commandline = 'chain.c32 hd0'
elseif derivative == "PXELINUX" then
	-- boot first hard drive
	commandline = 'chain.c32 hd0'
else
	printf("Do nothing\n")
	return 1
end

printf("\n  commandline for derivative:   %s\n\n", commandline)


-- Count down from 7
for time = 7, 1, -1 do
	printf("  Boot in %d second(s)...   \r", time)
	syslinux.sleep(1)
end

-- Boot
syslinux.run_command(commandline)

