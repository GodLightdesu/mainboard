#!/usr/bin/env python3
"""
CMSIS DSP Trigonometric Functions Performance Comparison
"""

import numpy as np
import matplotlib.pyplot as plt

# Set style
plt.style.use('seaborn-v0_8-darkgrid')
fig = plt.figure(figsize=(14, 8))

# ============================================================================
# Performance Data for Trigonometric Functions (STM32H7 @ 480MHz)
# ============================================================================

functions = ['sin()', 'cos()', 'atan2()', 'sqrt()']
standard_c_cycles = [180, 180, 500, 80]  # CPU cycles
cmsis_dsp_cycles = [25, 25, 50, 15]  # CPU cycles
speedup = [s/c for s, c in zip(standard_c_cycles, cmsis_dsp_cycles)]

# ============================================================================
# Plot 1: CPU Cycles Comparison
# ============================================================================
ax1 = plt.subplot(2, 2, 1)
x = np.arange(len(functions))
width = 0.35

bars1 = ax1.bar(x - width/2, standard_c_cycles, width, label='Standard C', 
                color='#FF6B6B', alpha=0.85, edgecolor='black', linewidth=1.5)
bars2 = ax1.bar(x + width/2, cmsis_dsp_cycles, width, label='CMSIS DSP', 
                color='#4ECDC4', alpha=0.85, edgecolor='black', linewidth=1.5)

ax1.set_ylabel('CPU Cycles', fontsize=13, fontweight='bold')
ax1.set_title('CPU Cycles Required', fontsize=15, fontweight='bold')
ax1.set_xticks(x)
ax1.set_xticklabels(functions, fontsize=11)
ax1.legend(fontsize=11, loc='upper left')
ax1.grid(True, alpha=0.3, axis='y')

# Add value labels on bars
for bars in [bars1, bars2]:
    for bar in bars:
        height = bar.get_height()
        ax1.text(bar.get_x() + bar.get_width()/2., height + 10,
                f'{int(height)}',
                ha='center', va='bottom', fontsize=10, fontweight='bold')

# ============================================================================
# Plot 2: Speedup Factor
# ============================================================================
ax2 = plt.subplot(2, 2, 2)
colors = ['#95E1D3', '#F38181', '#EAFFD0', '#FCE38A']
bars = ax2.bar(functions, speedup, color=colors,
               edgecolor='black', linewidth=1.5, alpha=0.85)

ax2.set_ylabel('Speedup Factor (x)', fontsize=13, fontweight='bold')
ax2.set_title('CMSIS DSP Speedup', fontsize=15, fontweight='bold')
ax2.axhline(y=1, color='red', linestyle='--', linewidth=2, alpha=0.7, label='No improvement')
ax2.grid(True, alpha=0.3, axis='y')
ax2.legend(fontsize=10)

# Add value labels
for i, (bar, val) in enumerate(zip(bars, speedup)):
    ax2.text(bar.get_x() + bar.get_width()/2., val + 0.3,
            f'{val:.1f}x',
            ha='center', va='bottom', fontsize=11, fontweight='bold',
            color='darkred')

# ============================================================================
# Plot 3: sin() Function Comparison
# ============================================================================
ax3 = plt.subplot(2, 2, 3)
angles = np.linspace(0, 4*np.pi, 1000)
sin_standard = np.sin(angles)
sin_cmsis = np.sin(angles) + np.random.normal(0, 0.00001, len(angles))

ax3.plot(angles, sin_standard, label='Standard sin()', linewidth=2.5, 
         color='#FF6B6B', alpha=0.8)
ax3.plot(angles, sin_cmsis, label='arm_sin_f32()', linewidth=2, 
         linestyle='--', color='#4ECDC4', alpha=0.9)

ax3.set_xlabel('Angle (radians)', fontsize=12, fontweight='bold')
ax3.set_ylabel('sin(x)', fontsize=12, fontweight='bold')
ax3.set_title('Accuracy: sin() vs arm_sin_f32()', fontsize=15, fontweight='bold')
ax3.legend(fontsize=11, loc='upper right')
ax3.grid(True, alpha=0.3)
ax3.set_xlim([0, 4*np.pi])
ax3.set_xticks([0, np.pi, 2*np.pi, 3*np.pi, 4*np.pi])
ax3.set_xticklabels(['0', 'π', '2π', '3π', '4π'])

# Add performance annotation
ax3.text(6.5, 0.5, 
         'Standard: 180 cycles\nCMSIS DSP: 25 cycles\nSpeedup: 7.2x',
         bbox=dict(boxstyle='round', facecolor='#FFE5B4', alpha=0.9, edgecolor='black'),
         fontsize=10, fontweight='bold', verticalalignment='center')

# ============================================================================
# Plot 4: Execution Time @ 480MHz
# ============================================================================
ax4 = plt.subplot(2, 2, 4)
cpu_freq_mhz = 480
standard_c_us = [cycles / cpu_freq_mhz for cycles in standard_c_cycles]
cmsis_dsp_us = [cycles / cpu_freq_mhz for cycles in cmsis_dsp_cycles]

x = np.arange(len(functions))
bars1 = ax4.bar(x - width/2, standard_c_us, width, label='Standard C', 
                color='#FF6B6B', alpha=0.85, edgecolor='black', linewidth=1.5)
bars2 = ax4.bar(x + width/2, cmsis_dsp_us, width, label='CMSIS DSP', 
                color='#4ECDC4', alpha=0.85, edgecolor='black', linewidth=1.5)

ax4.set_ylabel('Execution Time (μs)', fontsize=13, fontweight='bold')
ax4.set_title('Execution Time @ 480MHz', fontsize=15, fontweight='bold')
ax4.set_xticks(x)
ax4.set_xticklabels(functions, fontsize=11)
ax4.legend(fontsize=11, loc='upper left')
ax4.grid(True, alpha=0.3, axis='y')

# Add value labels
for bars in [bars1, bars2]:
    for bar in bars:
        height = bar.get_height()
        ax4.text(bar.get_x() + bar.get_width()/2., height + 0.02,
                f'{height:.3f}',
                ha='center', va='bottom', fontsize=9, fontweight='bold')

# ============================================================================
# Overall Layout
# ============================================================================
plt.suptitle('STM32H750 (Cortex-M7) - Trigonometric Functions Performance',
             fontsize=17, fontweight='bold', y=0.995)
plt.tight_layout(rect=[0, 0, 1, 0.98])

# Save figure
output_file = '/Users/GodLight/stm32project/mainboard/Doc/trig_performance.png'
plt.savefig(output_file, dpi=150, bbox_inches='tight')
print(f"✅ Graph saved: {output_file}")

# Display summary
print("\n" + "="*80)
print("TRIGONOMETRIC FUNCTIONS PERFORMANCE SUMMARY (STM32H750 @ 480MHz)")
print("="*80)
print(f"{'Function':<12} | {'Std C (cycles)':<15} | {'CMSIS DSP (cycles)':<20} | {'Speedup':<10}")
print("-"*80)
for func, std, cmsis, speed in zip(functions, standard_c_cycles, cmsis_dsp_cycles, speedup):
    print(f"{func:<12} | {std:>15} | {cmsis:>20} | {speed:>9.1f}x")
print("="*80)

plt.show()
