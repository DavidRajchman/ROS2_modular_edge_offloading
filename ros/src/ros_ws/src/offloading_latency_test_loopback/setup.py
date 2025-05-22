from setuptools import setup

package_name = 'offloading_latency_test_loopback'

setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Your Name',
    maintainer_email='user@example.com',
    description='Loopback latency testing for VHC-MEC offloading via Modular Gateway',
    license='Apache License 2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'vhc_node = offloading_latency_test_loopback.vhc_node:main',
            'mec_processing_node = offloading_latency_test_loopback.mec_processing_node:main',
        ],
    },
)
