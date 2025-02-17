# Lattice Crypto (C code from Microsoft)
LatticeCrypto is a post-quantum secure cryptography library based on the Ring-Learning with Errors (R-LWE) 
problem. The version 1.0 of the library implements the instantiation of Peikert's key exchange [1] due to 
Alkim, Ducas, Pöppelmann and Schwabe [2], and incorporates novel techniques to provide higher performance.

The library was developed by Microsoft Research for experimentation purposes. 

## Description
### Key Exchange kex.c
* @param clear_words Clears memory
* @param LatticeCrypto_initialize Initialize structure pLatticeCrypto with user-provided functions: RandomBytesFunction, ExtendableOutputFunction and StreamOutputFunction.
* @param LatticeCrypto_allocate Dynamically allocates memory for LatticeCrypto structure.
* @param LatticeCrypto_get_error_message Outputs error or success message for given CRYPTO_STATUS  
* @param encode_A Alice's message encryption 
* @param decode_A Alice's message decryption 
* @param encode_B Bob's message encryption
* @param decode_B Bob's message decryption 
* @param Abs Computes absolute value
* @param LDDecode Performs low-density decoding
* @param KeyGeneration_A Alice's 4096-byte SecretKeyA key generation and 1824-byte PublicKeyA computation
* @param SecretAgreement_B Bob's 2048-byte key generation from Alice's 1824 byte PublicKeyA and 256-bit shared secret computation
* @param SecretAgreement_A Computes shared secret SharedSecretA using Bob's 2048-byte public key PublicKeyB and Alice's 256-bit private key SecretKeyA.
## Installation
make ARCH=[x64/x86/ARM] CC=[gcc/clang] ASM=[TRUE/FALSE] AVX2=[TRUE/FALSE] GENERIC=[TRUE/FALSE]

# Quintuple (Python code from IBM)
This is an implementation of IBM's Quantum Experience in simulation; a 5-qubit quantum computer with a limited set of gates "the world’s first quantum computing platform delivered via the IBM Cloud". Their implementation is available at [http://www.research.ibm.com/quantum/](http://www.research.ibm.com/quantum/).

This code allows you to execute code printed from the Quantum Composer in the following syntax: 

|   description       |    usage           |
| ------------- | ------------- |
|available qubit list| q[0], q[1], q[2], q[3], q[4]|
|1-qubit gate list| h,t,tdg,s,sdg,x,y,z,id|
|1-qubit gate action| gate q[i];|
|2-qubit CNOT gate list | cx |
| 2-qubit CNOT gate action| cx q[control], q[target];|
| measurement operation list | measure, bloch|
| measurement operation action | operation q[i]; |

It is much easier to dig into the internals of how the quantum computer computes by seeing and tracing the linear algebra representation of gates and states and their interactions as desired–for IBM's examples or for one's own code.

100% of the examples on the IBM tutorial are provided here, tested for and supported, and many addition tests and examples are provided. In fact, the implementation of the 5-qubit quantum computer simulator is only 675 lines, with approximately twice as many lines of test programs and examples provided.

Check out any of the test functions for example usage, and the ```Programs``` class contains many example programs in IBM's syntax all available in one place.

If you make use of this work, please cite my paper available on the physics arXiv as [Quintuple: a Python 5-qubit quantum computer simulator to facilitate cloud quantum computing](http://arxiv.org/abs/1606.09225). For the making of story of this code, along with some pointers to resources on quantum computation, [check out my blog post](https://codexgalactic.com/2016/05/21/5-qubit-quantum-computing-simulator/).


## Example usage 
```
from QuantumComputer import *
ghz_example_code="""h q[0];
		h q[1];
		x q[2];
		cx q[1], q[2];
		cx q[0], q[2];
		h q[0];
		h q[1];
		h q[2];"""
qc=QuantumComputer()
qc.execute(ghz_example_code)
Probability.pretty_print_probabilities(qc.qubits.get_quantum_register_containing("q0").get_state())
```
This will print
```
|psi>=0.70710678118654724|000>+-0.70710678118654724|111>
Pr(|000>)=0.500000; Pr(|111>)=0.500000; 
```

Or, using the swap Qubits example IBM tutorial Section IV, Page 2
```
swap_example_code="""x q[2];
		cx q[1], q[2];
		h q[1];
		h q[2];
		cx q[1], q[2];
		h q[1];
		h q[2];
		cx q[1], q[2];
		measure q[1];
		measure q[2];"""
qc.reset()
qc.execute(swap_example_code)
Probability.pretty_print_probabilities(qc.qubits.get_quantum_register_containing("q2").get_state())
```
will print
```
|psi>=|10>
Pr(|10>)=1.000000; 
<state>=-1.000000
```

We'll continue with this example in pure python below.

Note: using IBM's measurment code ```measure q[0];``` will actually collapse the state, but for convenience the internal state before collapse is stored in qubit.get_noop(). Nature doesn't give this to us, but I can give it to you!


# Pure python quantum computing machinery 
Quantum computing operations can also be done in pure python, either with the QuantumComputer machinery or by directly manipulating gates.
## QuantumComputer machinery

```
# Swap Qubits example IBM tutorial Section IV, Page 2
qc=QuantumComputer()
qc.apply_gate(Gate.X,"q2")
qc.apply_two_qubit_gate_CNOT("q1","q2")
qc.apply_gate(Gate.H,"q1")
qc.apply_gate(Gate.H,"q2")
qc.apply_two_qubit_gate_CNOT("q1","q2")
qc.apply_gate(Gate.H,"q1")
qc.apply_gate(Gate.H,"q2")
qc.apply_two_qubit_gate_CNOT("q1","q2")
qc.measure("q1")
qc.measure("q2")
Probability.pretty_print_probabilities(qc.qubits.get_quantum_register_containing("q1").get_state())
```
Will print
```
|psi>=|10>
Pr(|10>)=1.000000; 
<state>=-1.000000
```

## Working with Individual States and gates

Note that states are combined by using the Kronecker product. Gates that operate on entangled states are composed from single qubit gates by the Kronecker product of the gate with the Identity. See the internals of ```qc.apply_gate``` or ```qc.apply_two_qubit_gate_CNOT``` for general examples, or feel free to use them instead.

```
# Swap Qubits example IBM tutorial Section IV, Page 2
q1=State.zero_state
q2=State.zero_state
q2=Gate.X*q2
new_state=Gate.CNOT2_01*np.kron(q1,q2)
H2_0=np.kron(Gate.H,Gate.eye)
H2_1=np.kron(Gate.eye,Gate.H)
new_state=H2_0*new_state
new_state=H2_1*new_state
new_state=Gate.CNOT2_01*new_state
new_state=H2_0*new_state
new_state=H2_1*new_state
new_state=Gate.CNOT2_01*new_state
Probability.pretty_print_probabilities(new_state)
```

Will print 
```
|psi>=0.99999999999999967|10>
Pr(|10>)=1.000000;
<state>=-1.000000
```

 This final manner of working with the library provides the most complete mathematical understanding of what's going on. Any individual state or gate can be printed, and it is clear how entanglement is represented as this is not done under the hood in this scenario.
