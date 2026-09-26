"""Verify the official provider-specific root and optional live TLS 1.2."""
import argparse
import hashlib
import socket
import ssl
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.verification import PolicyBuilder, Store, VerificationError

ROOT = Path(__file__).resolve().parents[1]
PEM = ROOT / 'data/cert/isrg_root_x2.pem'
EXPECTED_DER = '69729b8e15a86efc177a57afb7171dfc64add28c2fca8cf1507e34453ccb1470'
EXPECTED_PEM = 'a13d881e11fe6df181b53841f9fa738a2d7ca9ae7be3d53c866f722b4242b013'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--live', action='store_true')
    args = parser.parse_args()
    pem = PEM.read_bytes()
    cert = x509.load_pem_x509_certificate(pem)
    assert hashlib.sha256(pem).hexdigest() == EXPECTED_PEM
    assert cert.fingerprint(hashes.SHA256()).hex() == EXPECTED_DER
    assert cert.subject == cert.issuer
    assert cert.extensions.get_extension_for_class(x509.BasicConstraints).value.ca
    assert cert.extensions.get_extension_for_class(x509.KeyUsage).value.key_cert_sign
    cert.public_key().verify(cert.signature, cert.tbs_certificate_bytes,
                             ec.ECDSA(cert.signature_hash_algorithm))
    print('PASS: official ISRG Root X2 fingerprint, exact PEM, CA constraints and self-signature')
    print('Validity:', cert.not_valid_before_utc, 'through', cert.not_valid_after_utc)
    if not args.live:
        return
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.load_verify_locations(cafile=str(PEM))
    ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    host = 'stats-btc.heliospool.com'
    with socket.create_connection((host, 443), timeout=10) as raw:
        with ctx.wrap_socket(raw, server_hostname=host) as conn:
            assert conn.version() == 'TLSv1.2'
            print('PASS: live TLS 1.2, only ISRG Root X2, hostname verification enabled')
            chain = [x509.load_pem_x509_certificate(c.public_bytes().encode())
                     for c in conn._sslobj.get_unverified_chain()]
    policy = PolicyBuilder().store(Store([cert]))
    policy.build_server_verifier(x509.DNSName(host)).verify(chain[0], chain[1:])
    try:
        policy.build_server_verifier(x509.DNSName('wrong.example.invalid')).verify(chain[0], chain[1:])
    except VerificationError as error:
        assert 'subjectAltName' in str(error), str(error)
        print('PASS: independently verified live certificate chain rejects wrong hostname')
    else:
        raise AssertionError('hostname mismatch accepted')

if __name__ == '__main__':
    main()
