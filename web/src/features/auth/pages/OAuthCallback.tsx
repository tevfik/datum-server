import { useEffect, useState } from 'react';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { useAuth } from '@/context/AuthContext';
import { Loader2, AlertCircle } from 'lucide-react';

/**
 * OAuthCallback — handles the redirect from the backend OAuth callback.
 *
 * The backend redirects here with query params:
 *   token, refresh_token, user_id, email, role
 *
 * We read them, call login(), and navigate to the dashboard.
 */
export default function OAuthCallback() {
    const [searchParams] = useSearchParams();
    const { login } = useAuth();
    const navigate = useNavigate();
    const [error, setError] = useState('');

    useEffect(() => {
        const token = searchParams.get('token');
        const refreshToken = searchParams.get('refresh_token');
        const userId = searchParams.get('user_id');
        const email = searchParams.get('email');
        const role = searchParams.get('role');

        if (!token || !userId || !email) {
            setError('OAuth login failed — invalid callback parameters. Please try again.');
            return;
        }

        // Decode the JWT to get expiry (exp claim)
        let expiresAt = '';
        try {
            const payload = JSON.parse(atob(token.split('.')[1]));
            if (payload.exp) {
                expiresAt = new Date(payload.exp * 1000).toISOString();
            }
        } catch {
            // Fallback: 15 minutes from now
            expiresAt = new Date(Date.now() + 15 * 60 * 1000).toISOString();
        }

        login(
            token,
            refreshToken ?? undefined,
            { id: userId, email, role: role ?? 'user' },
            expiresAt,
            true, // remember me — OAuth users expect persistence
        );

        navigate('/', { replace: true });
    }, []);

    if (error) {
        return (
            <div className="flex h-screen items-center justify-center bg-muted/40 px-4">
                <div className="flex max-w-sm flex-col items-center gap-4 text-center">
                    <AlertCircle className="h-10 w-10 text-destructive" />
                    <p className="text-sm text-destructive">{error}</p>
                    <a href="/login" className="text-sm text-primary underline underline-offset-4">
                        Back to login
                    </a>
                </div>
            </div>
        );
    }

    return (
        <div className="flex h-screen items-center justify-center bg-muted/40">
            <div className="flex flex-col items-center gap-3 text-muted-foreground">
                <Loader2 className="h-8 w-8 animate-spin" />
                <p className="text-sm">Signing you in…</p>
            </div>
        </div>
    );
}
