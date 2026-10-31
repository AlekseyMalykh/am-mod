# AC50 charging control: question for Akai or a developer

I'm developing a nonpersistent SD-launched mod for the Akai MPC Sample,
stock firmware 1.3.0.12.

Is there a supported Linux-to-control-surface-MCU command, such as SysEx,
that inhibits battery charging while keeping USB Audio input and output active?
The intended policies are stock charging and charging only below 15% battery.

Static analysis of the bundled MCU image shows a BQ24296/97-compatible charger
register map and an autonomous policy that sets CHG_CONFIG again. The Type-C
driver reads a TUSB320-compatible CURRENT_MODE_DETECT field. Neither the known
host parameters nor the available Linux power_supply properties have exposed
a charge-inhibit control so far.

If such a command exists, could you provide its packet format, state/readback
semantics, and how to restore the stock policy without flashing the MCU?
